/*=========================================================================

  Project                 : Icarus
  Module                  : vtkDsmManager.cxx

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
#include "vtkDsmManager.h"
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
#include "vtkToolkits.h" // For VTK_USE_MPI
#ifdef VTK_USE_MPI
 #include "vtkMPI.h"
 #include "vtkMPIController.h"
 #include "vtkMPICommunicator.h"
#endif
// Otherwise
#include "vtkMultiProcessController.h"
//
#include "vtkDsmProxyHelper.h"
#include "XdmfGenerator.h"
#include "vtkPVOptions.h"
#include "vtkClientSocket.h"

#include "vtkMultiThreader.h"
#include "vtkConditionVariable.h"

//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkDsmManager, "$Revision$");
vtkStandardNewMacro(vtkDsmManager);
vtkCxxSetObjectMacro(vtkDsmManager, Controller, vtkMultiProcessController);

//----------------------------------------------------------------------------
VTK_EXPORT VTK_THREAD_RETURN_TYPE vtkDsmManagerNotificationThread(void *arg)
{
  vtkDsmManager *dsmManager = static_cast<vtkDsmManager*>(
      static_cast<vtkMultiThreader::ThreadInfo*>(arg)->UserData);
  dsmManager->NotificationThread();
  return(VTK_THREAD_RETURN_VALUE);
}

//----------------------------------------------------------------------------
struct vtkDsmManager::vtkDsmManagerInternals
{
  vtkDsmManagerInternals() {
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

  ~vtkDsmManagerInternals() {
//    if (this->NotificationThread->IsThreadActive(this->NotificationThreadID)) {
//      this->NotificationThread->TerminateThread(this->NotificationThreadID);
//    }
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
vtkDsmManager::vtkDsmManager()
{
  this->UpdatePiece             = 0;
  this->UpdateNumPieces         = 0;
  //
#ifdef VTK_USE_MPI
  this->Controller              = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
#endif
  this->XdmfTemplateDescription = NULL;
  this->XdmfDescription         = NULL;
  this->HelperProxyXMLString    = NULL;
  this->DsmManager              = new H5FDdsmManager();
  this->DsmManagerInternals     = new vtkDsmManagerInternals;
}

//----------------------------------------------------------------------------
vtkDsmManager::~vtkDsmManager()
{ 
  this->SetXdmfTemplateDescription(NULL);
  this->SetHelperProxyXMLString(NULL);
  if (this->DsmManager) delete this->DsmManager;  
  this->DsmManager = NULL;
  if (this->DsmManagerInternals) delete this->DsmManagerInternals;
  this->DsmManagerInternals = NULL;
#ifdef VTK_USE_MPI
  this->SetController(NULL);
#endif
}

//----------------------------------------------------------------------------
void vtkDsmManager::CheckMPIController()
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
          vtkstd::cout << "MPI_THREAD_MULTIPLE not set, you may need to recompile your "
            << "MPI distribution with threads enabled" << vtkstd::endl;
        }
        else {
          vtkstd::cout << "MPI_THREAD_MULTIPLE is OK (DSM override)" << vtkstd::endl;
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
void* vtkDsmManager::NotificationThread()
{
  this->DsmManagerInternals->SignalNotifThreadCreated();
  this->WaitForConnection();
  this->DsmManagerInternals->NotificationSocket->Send("C", 1);

  while (this->DsmManager->GetIsConnected()) {
    if (this->WaitForUnlock() != H5FD_DSM_FAIL) {
      this->DsmManagerInternals->NotificationSocket->Send("N", 1);
      this->WaitForUpdated();
    }
  }
  // TODO
  // good cleanup ??
  return((void *)this);
}

//----------------------------------------------------------------------------
void vtkDsmManager::SignalUpdated()
{
  this->DsmManagerInternals->UpdatedMutex->Lock();

  this->DsmManagerInternals->IsUpdated = true;
  this->DsmManagerInternals->UpdatedCond->Signal();
  vtkDebugMacro("Sent updated condition signal");

  this->DsmManagerInternals->UpdatedMutex->Unlock();

}

//----------------------------------------------------------------------------
void vtkDsmManager::WaitForUpdated()
{
  this->DsmManagerInternals->UpdatedMutex->Lock();

  while (!this->DsmManagerInternals->IsUpdated) {
    vtkDebugMacro("Thread going into wait for pipeline updated...");
    this->DsmManagerInternals->UpdatedCond->Wait(this->DsmManagerInternals->UpdatedMutex);
    vtkDebugMacro("Thread received updated signal");
  }
  if (this->DsmManagerInternals->IsUpdated) {
    this->DsmManagerInternals->IsUpdated = false;
  }

  this->DsmManagerInternals->UpdatedMutex->Unlock();
}

//----------------------------------------------------------------------------
int vtkDsmManager::Create()
{
  this->CheckMPIController();

  //
  // Get the raw MPI_Comm handle
  //
  vtkMPICommunicator *communicator
  = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator());
  this->DsmManager->SetMpiComm(*communicator->GetMPIComm()->GetHandle());

#ifdef VTK_USE_MPI
  this->UpdatePiece     = this->Controller->GetLocalProcessId();
  this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
#else
  this->UpdatePiece     = 0;
  this->UpdateNumPieces = 1;
  vtkErrorMacro(<<"No MPI");
  return 0;
#endif

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  vtkPVOptions *pvOptions = pm->GetOptions();
  const char *pvClientHostName = pvOptions->GetClientHostName();
  int notificationPort = VTK_DSM_MANAGER_DEFAULT_NOTIFICATION_PORT;
  if ((this->UpdatePiece == 0) && pvClientHostName && pvClientHostName[0]) {
    int r, tryConnect = 0;
    this->DsmManagerInternals->NotificationSocket = vtkSmartPointer<vtkClientSocket>::New();
    do {
      vtkstd::cout << "Creating notification socket to "
          << pvClientHostName << " on port " << notificationPort << "...";
      r = this->DsmManagerInternals->NotificationSocket->ConnectToServer(pvClientHostName,
          notificationPort);
      if (r == 0) {
        vtkstd::cout << "Connected" << vtkstd::endl;
      } else {
        vtkstd::cout << "Failed to connect" << vtkstd::endl;
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
  if (this->DsmManager->Create()==H5FD_DSM_SUCCESS) {
    // Set mode to Asynchronous so that we can open/close whenever we like
    this->DsmManager->GetDsmBuffer()->SetSychronizationCount(0);
    return H5FD_DSM_SUCCESS;
  }
  return H5FD_DSM_FAIL;
}

//----------------------------------------------------------------------------
int vtkDsmManager::Destroy()
{
  return(this->DsmManager->Destroy());
}

//----------------------------------------------------------------------------
int vtkDsmManager::Publish()
{
  if (this->UpdatePiece == 0) {
    this->DsmManagerInternals->NotificationThreadID =
        this->DsmManagerInternals->NotificationThread->SpawnThread(
        vtkDsmManagerNotificationThread, (void *) this);
    this->DsmManagerInternals->WaitForNotifThreadCreated();
  }
  this->DsmManager->Publish();
  this->DsmManager->SetIsAsynchronous(1);
  return(1);
}

//----------------------------------------------------------------------------
void vtkDsmManager::GenerateXdmfDescription()
{
  XdmfGenerator *xdmfGenerator = new XdmfGenerator();

  if (this->GetDsmManager()) {
    xdmfGenerator->SetDsmManager(this->GetDsmManager());
    xdmfGenerator->Generate((const char*)this->GetXdmfTemplateDescription(), "DSM:file.h5");
  } else {
    vtkErrorMacro("GenerateXdmfDescription failed: no DSM buffer found")
  }

  vtkDebugMacro(<< xdmfGenerator->GetGeneratedFile());
  this->SetXdmfDescription(xdmfGenerator->GetGeneratedFile());

  delete xdmfGenerator;
}

//----------------------------------------------------------------------------
int vtkDsmManager::ReadConfigFile()
{
  this->CheckMPIController();
  //
  // Get the raw MPI_Comm handle
  //
  vtkMPICommunicator *communicator
  = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator());
  this->DsmManager->SetMpiComm(*communicator->GetMPIComm()->GetHandle());

#ifdef VTK_USE_MPI
  this->UpdatePiece     = this->Controller->GetLocalProcessId();
  this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
#else
  this->UpdatePiece     = 0;
  this->UpdateNumPieces = 1;
  vtkErrorMacro(<<"No MPI");
  return 0;
#endif

  return(this->DsmManager->ReadConfigFile());
}

//----------------------------------------------------------------------------
void vtkDsmManager::SetHelperProxyXMLString(const char *xmlstring)
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
void vtkDsmManager::RegisterHelperProxy(const char *xmlstring)
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
void vtkDsmManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
