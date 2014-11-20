/*=========================================================================

  Project                 : Icarus
  Module                  : vtkAbstractDsmManager.cxx

  Authors:
     John Biddiscombe     Jerome Soumagne
     biddisco@cscs.ch     soumagne@cscs.ch

  Copyright (C) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing
  1) This copyright notice appears on all copies of source code
  2) An acknowledgment appears with any substantial usage of the code
  3) If this code is contributed to any other open source project, it
  must not be reformatted such that the indentation, bracketing or
  overall style is modified significantly.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  This work has received funding from the European Community's Seventh
  Framework Programme (FP7/2007-2013) under grant agreement 225967 “NextMuSE”

=========================================================================*/
#include "vtkAbstractDsmManager.h"
//
#include "vtkObjectFactory.h"
//
#include "vtkProcessModule.h"
#include "vtkSMProxyManager.h"
#include "vtkClientServerInterpreterInitializer.h"
#include "vtkSMProxyDefinitionManager.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkPVSessionServer.h"
#include "vtkPVSessionCore.h"
//
// For PARAVIEW_USE_MPI
#include "vtkPVConfig.h"
#ifdef PARAVIEW_USE_MPI
 #include "vtkMPI.h"
 #include "vtkMPIController.h"
 #include "vtkMPICommunicator.h"
#endif
// Otherwise
#include "vtkMultiProcessController.h"
//
#include "vtkDsmProxyHelper.h"
#include "vtkPVServerOptions.h"
#include "vtkClientSocket.h"

#include "vtkMultiThreader.h"
#include "vtkConditionVariable.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkAbstractDsmManager);
vtkCxxSetObjectMacro(vtkAbstractDsmManager, Controller, vtkMultiProcessController);

//----------------------------------------------------------------------------
struct vtkAbstractDsmManager::vtkAbstractDsmManagerInternals
{
  vtkAbstractDsmManagerInternals() {
    this->NotificationThread = vtkSmartPointer<vtkMultiThreader>::New();

    // Updated event
    this->IsUpdated = false;
    this->UpdatedCond = vtkSmartPointer<vtkConditionVariable>::New();
    this->UpdatedMutex = vtkSmartPointer<vtkMutexLock>::New();

    // NotifThreadCreated event
    this->IsNotifThreadCreated = false;
    this->NotifThreadCreatedCond = vtkSmartPointer<vtkConditionVariable>::New();
    this->NotifThreadCreatedMutex = vtkSmartPointer<vtkMutexLock>::New();
  }

  ~vtkAbstractDsmManagerInternals() {
    if (this->NotificationThread->IsThreadActive(this->NotificationThreadID)) {
      this->NotificationThread->TerminateThread(this->NotificationThreadID);
    }
  }

  void SignalNotifThreadCreated() {
    this->NotifThreadCreatedMutex->Lock();
    this->IsNotifThreadCreated = true;
    this->NotifThreadCreatedCond->Signal();
    this->NotifThreadCreatedMutex->Unlock();
  }

  void WaitForNotifThreadCreated() {
    this->NotifThreadCreatedMutex->Lock();
    while (!this->IsNotifThreadCreated) {
      this->NotifThreadCreatedCond->Wait(this->NotifThreadCreatedMutex);
    }
    this->NotifThreadCreatedMutex->Unlock();
  }

  vtkSmartPointer<vtkClientSocket>  NotificationSocket;
  vtkSmartPointer<vtkMultiThreader> NotificationThread;
  int NotificationThreadID;

  // Updated event
  bool                                  IsUpdated;
  vtkSmartPointer<vtkMutexLock>         UpdatedMutex;
  vtkSmartPointer<vtkConditionVariable> UpdatedCond;

  // NotifThreadCreated event
  bool                                  IsNotifThreadCreated;
  vtkSmartPointer<vtkMutexLock>         NotifThreadCreatedMutex;
  vtkSmartPointer<vtkConditionVariable> NotifThreadCreatedCond;
};

//----------------------------------------------------------------------------
vtkAbstractDsmManager::vtkAbstractDsmManager()
{
  this->UpdatePiece        = 0;
  this->UpdateNumPieces    = 0;
  this->DSMPollingFunction = nullptr;

#ifdef VTK_USE_MPI
  this->Controller              = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
#endif
  this->HelperProxyXMLString    = NULL;
  this->DsmManagerInternals     = new vtkAbstractDsmManagerInternals;
}

//----------------------------------------------------------------------------
vtkAbstractDsmManager::~vtkAbstractDsmManager()
{ 
  this->SetHelperProxyXMLString(NULL);
  if (this->DsmManagerInternals) delete this->DsmManagerInternals;
  this->DsmManagerInternals = NULL;
#ifdef VTK_USE_MPI
  this->SetController(NULL);
#endif
}

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::CheckMPIController()
{
  if (this->Controller->IsA("vtkDummyController"))
  {
    vtkDebugMacro("Running vtkDummyController : replacing it");
    int flag = 0;
    MPI_Initialized(&flag);
    if (flag == 0)
    {
      vtkDebugMacro("Running without MPI, attempting to initialize ");
      //int argc = 1;
      //const char *argv = "D:\\cmakebuild\\pv-shared\\bin\\RelWithDebInfo\\paraview.exe";
      //char **_argv = (char**) &argv;
      int provided, rank, size;
      MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      MPI_Comm_size(MPI_COMM_WORLD, &size);
      //
      if (rank == 0) {
        if (provided != MPI_THREAD_MULTIPLE) {
          std::cout << "MPI_THREAD_MULTIPLE not set, you may need to recompile your "
            << "MPI distribution with threads enabled" << std::endl;
        }
        else {
          std::cout << "MPI_THREAD_MULTIPLE is OK (DSM override)" << std::endl;
        }
      }
    }
    //
    vtkDebugMacro("Setting Global MPI controller");
    vtkSmartPointer<vtkMPIController> controller = vtkSmartPointer<vtkMPIController>::New();
    if (flag == 0) controller->Initialize(NULL, NULL, 1);
    this->SetController(controller);
    vtkMPIController::SetGlobalController(controller);
  }
}

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::NotificationThread()
{
  unsigned int notification;

  this->DsmManagerInternals->SignalNotifThreadCreated();
  this->WaitForConnection();
  if (this->UpdatePiece==0) {
    notification = DSM_NOTIFY_CONNECTED;
    this->SendNotification(notification, sizeof(notification));
  }
  this->WaitForUpdated();

  if (!DSMPollingFunction) {
    std::cout << "Polling function has not been assigned, FATAL " << std::endl;
    return;
  }
  while (this->DSMPollingFunction(&notification) != 0) {
    this->SendNotification(notification, sizeof(notification));
    std::cout << "Entering WaitForUpdated " << std::endl;
    this->WaitForUpdated();
    std::cout << "done WaitForUpdate " << std::endl;
  }
  return;
}
//----------------------------------------------------------------------------
void vtkAbstractDsmManager::WaitForNotifThreadCreated()
{
  this->DsmManagerInternals->WaitForNotifThreadCreated();
}

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::SendNotification(int notification, int size)
{
  if (this->UpdatePiece==0) {
    std::cout << "Sending notification " << notification << " from rank " << this->UpdatePiece << std::endl;
    this->DsmManagerInternals->NotificationSocket->Send(&notification, size);
  }
}

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::SignalUpdated()
{
  this->DsmManagerInternals->UpdatedMutex->Lock();

  this->DsmManagerInternals->IsUpdated = true;
  this->DsmManagerInternals->UpdatedCond->Signal();
  vtkDebugMacro("Sent updated condition signal on rank " << this->UpdatePiece);

  this->DsmManagerInternals->UpdatedMutex->Unlock();

}

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::WaitForUpdated()
{
  std::cout << "Entering WaitForUpdated on rank " << this->UpdatePiece << std::endl;
  this->DsmManagerInternals->UpdatedMutex->Lock();

  if (!this->DsmManagerInternals->IsUpdated) {
    vtkDebugMacro("Thread going into wait for pipeline updated...");
    this->DsmManagerInternals->UpdatedCond->Wait(this->DsmManagerInternals->UpdatedMutex);
    vtkDebugMacro("Thread received updated signal");
  }
  if (this->DsmManagerInternals->IsUpdated) {
    this->DsmManagerInternals->IsUpdated = false;
  }

  this->DsmManagerInternals->UpdatedMutex->Unlock();
  std::cout << "done WaitForUpdate on rank " << this->UpdatePiece << std::endl;
}

//----------------------------------------------------------------------------
int vtkAbstractDsmManager::Create()
{
  this->CheckMPIController();

  //
  // Get the raw MPI_Comm handle
  //
  vtkMPICommunicator *communicator
  = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator());

#ifdef VTK_USE_MPI
  this->UpdatePiece     = this->Controller->GetLocalProcessId();
  this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
  std::cout << "Create in DSM manager rank " << this->UpdatePiece << " of " << this->UpdateNumPieces << std::endl;
#else

  this->UpdatePiece     = 0;
  this->UpdateNumPieces = 1;
  vtkErrorMacro(<<"No MPI");
  return 0;
#endif

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();

  vtkPVServerOptions *pvOptions = vtkPVServerOptions::SafeDownCast(pm->GetOptions());

  const char *pvClientHostName = pvOptions->GetClientHostName();
  int notificationPort = VTK_DSM_MANAGER_DEFAULT_NOTIFICATION_PORT;
  if ((this->UpdatePiece == 0) && pvClientHostName && pvClientHostName[0]) {
    int r, tryConnect = 0;
    this->DsmManagerInternals->NotificationSocket = vtkSmartPointer<vtkClientSocket>::New();
    do {
      std::cout << "Creating notification socket to "
          << pvClientHostName << " on port " << notificationPort << "...";
      r = this->DsmManagerInternals->NotificationSocket->ConnectToServer(pvClientHostName,
          notificationPort);
      if (r == 0) {
        std::cout << "Connected" << std::endl;
      } else {
        std::cout << "Failed to connect" << std::endl;
        tryConnect++;
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
      }
    } while (r < 0 && tryConnect < 5);
    if (r < 0) {
      this->DsmManagerInternals->NotificationSocket = NULL;
      return(-1);
    }
  }
/*
  if (this->DsmManager->Create()==H5FD_DSM_SUCCESS) {
    // Set mode to Asynchronous so that we can open/close whenever we like
    this->DsmManager->GetDsmBuffer()->SetSychronizationCount(0);
    return H5FD_DSM_SUCCESS;
  }
  return H5FD_DSM_FAIL;
*/
  return 1;
}

//----------------------------------------------------------------------------
int vtkAbstractDsmManager::Destroy()
{
  return 1;
}

//----------------------------------------------------------------------------
int vtkAbstractDsmManager::Publish()
{ return 1; }

//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::SetHelperProxyXMLString(const char *xmlstring)
{
  if ( this->HelperProxyXMLString == NULL && xmlstring == NULL) { return; }
  if ( this->HelperProxyXMLString && xmlstring && (!strcmp(this->HelperProxyXMLString,xmlstring))) { return; } 
  if ( this->HelperProxyXMLString) { delete [] this->HelperProxyXMLString; } 
  if (xmlstring) {
    size_t n = strlen(xmlstring) + 1;
    char *cp1 =  new char[n];
    const char *cp2 = (xmlstring);
    this->HelperProxyXMLString = cp1;
    do { *cp1++ = *cp2++; } while ( --n );
    // maybe we should just register it and not bother saving the string?
    this->RegisterHelperProxy(xmlstring);
  }
  else {
    this->HelperProxyXMLString = NULL;
  }
}

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::RegisterHelperProxy(const char *xmlstring)
{
  // Register a constructor function for the DsmProxyHelper
  vtkClientServerInterpreterInitializer *Initializer = vtkClientServerInterpreterInitializer::GetInitializer();
  Initializer->RegisterCallback(&::DsmProxyHelperInit);

  // Pass the DsmProxyHelper XML into the proxy manager for use by NewProxy(...)
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  vtkProcessModule::ProcessTypes ptype = pm->GetProcessType();
  if (ptype == vtkProcessModule::PROCESS_CLIENT) {
    vtkSMProxyManager *pm = vtkSMProxyManager::GetProxyManager();
    pm->GetActiveSession();
    vtkSMSessionProxyManager* pxm = pm->GetActiveSessionProxyManager();
    vtkSMProxyDefinitionManager* pxdm = pxm->GetProxyDefinitionManager();
    pxdm->LoadConfigurationXMLFromString(xmlstring);
  }
  else /* if (ptype == vtkProcessModule::PROCESS_SERVER) */ {
    vtkPVSessionServer *serverSession =
        vtkPVSessionServer::SafeDownCast(pm->GetActiveSession());
    vtkSIProxyDefinitionManager* pxdm =
        serverSession->GetSessionCore()->GetProxyDefinitionManager();
    pxdm->LoadConfigurationXMLFromString(xmlstring);
  }
}

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::SetLocalBufferSizeMBytes(int size) {}
//----------------------------------------------------------------------------
int vtkAbstractDsmManager:: GetLocalBufferSizeMBytes() { return 1; }

//----------------------------------------------------------------------------
int vtkAbstractDsmManager::Connect() { return 1; }

//----------------------------------------------------------------------------
int vtkAbstractDsmManager::Disconnect() { return 1; }

//----------------------------------------------------------------------------
int vtkAbstractDsmManager::Unpublish() { return 1; }

//----------------------------------------------------------------------------
int vtkAbstractDsmManager::OpenCollective() { return 1; }

//----------------------------------------------------------------------------
int vtkAbstractDsmManager::CloseCollective() { return 1; }

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::UpdateSteeredObjects() { return; }

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::SetSteeringCommand(char *command) 
{}

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::SetSteeringValues(const char *name, int numberOfElements, int *values)
{}

//----------------------------------------------------------------------------
void vtkAbstractDsmManager::SetSteeringValues(const char *name, int numberOfElements, double *values)
{}

//----------------------------------------------------------------------------
int vtkAbstractDsmManager::GetSteeringValues(const char *name, int numberOfElements, int *values)
{ return 1; }

//----------------------------------------------------------------------------
int vtkAbstractDsmManager::GetSteeringValues(const char *name, int numberOfElements, double *values)
{ return 1; }

//----------------------------------------------------------------------------



