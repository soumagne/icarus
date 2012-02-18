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
#include <vtksys/SystemTools.hxx>
#include <vtksys/RegularExpression.hxx>
#include <vtkstd/vector>
//
#include "vtkSmartPointer.h"
#include "vtkProcessModule.h"
#include "vtkSMObject.h"
#include "vtkSMProxyManager.h"
#include "vtkClientServerInterpreter.h"
#include "vtkClientServerInterpreterInitializer.h"
#include "vtkSMProxyDefinitionManager.h"
#include "vtkSMSessionProxyManager.h"
//
#ifdef VTK_USE_MPI
#include "vtkMPI.h"
#include "vtkMPIController.h"
#include "vtkMPICommunicator.h"
vtkCxxSetObjectMacro(vtkDsmManager, Controller, vtkMultiProcessController);
#endif
//
#include "vtkDsmProxyHelper.h"
#include "XdmfGenerator.h"
#include "vtkPVOptions.h"
#include "vtkClientSocket.h"

#ifdef _WIN32
  #include <windows.h>
#else
  #include <pthread.h>
#endif

//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkDsmManager, "$Revision$");
vtkStandardNewMacro(vtkDsmManager);
//----------------------------------------------------------------------------
struct vtkDsmManager::vtkDsmManagerInternals
{
  vtkDsmManagerInternals() {
    this->NotificationThreadPtr = 0;
#ifdef _WIN32
    this->NotificationThreadHandle = NULL;
#endif

    this->IsUpdated = false;
#ifdef _WIN32
  #if (WINVER < _WIN32_WINNT_LONGHORN)
    this->UpdatedEvent = CreateEvent(NULL, TRUE, FALSE, TEXT("UpdatedEvent"));
  #else
    InitializeCriticalSection  (&this->UpdatedCritSection);
    InitializeConditionVariable(&this->UpdatedCond);
  #endif
#else
    pthread_mutex_init(&this->UpdatedMutex, NULL);
    pthread_cond_init (&this->UpdatedCond, NULL);
#endif
  }
  ~vtkDsmManagerInternals() {
#ifdef _WIN32
  #if (WINVER < _WIN32_WINNT_LONGHORN)
    CloseHandle(this->UpdatedEvent);
  #else
    DeleteCriticalSection(&this->UpdatedCritSection);
  #endif
#else
    pthread_mutex_destroy(&this->UpdatedMutex);
    pthread_cond_destroy (&this->UpdatedCond);
#endif
  }
  vtkSmartPointer<vtkClientSocket> NotificationSocket;

#ifdef _WIN32
  DWORD  NotificationThreadPtr;
  HANDLE NotificationThreadHandle;
#else
  pthread_t NotificationThreadPtr;
#endif

  bool                    IsUpdated;
#ifdef _WIN32
#if (WINVER < _WIN32_WINNT_LONGHORN)
  HANDLE                  UpdatedEvent;
#else
  CRITICAL_SECTION        UpdatedCritSection;
  CONDITION_VARIABLE      UpdatedCond;
#endif
#else
  pthread_mutex_t         UpdatedMutex;
  pthread_cond_t          UpdatedCond;
#endif
};

//----------------------------------------------------------------------------
extern "C"{
#ifdef _WIN32
  VTK_EXPORT DWORD WINAPI vtkDsmManagerNotificationThread(void *dsmManagerPtr)
  {
    vtkDsmManager *dsmManager = (vtkDsmManager *)dsmManagerPtr;
    dsmManager->NotificationThread();
    return(0);
  }
#else
  VTK_EXPORT void *
  vtkDsmManagerNotificationThread(void *dsmManagerPtr)
  {
    vtkDsmManager *dsmManager = (vtkDsmManager *)dsmManagerPtr;
    return(dsmManager->NotificationThread());
  }
#endif
}

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
void
vtkDsmManager::CheckMPIController()
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
  this->WaitForConnection();
  this->DsmManagerInternals->NotificationSocket->Send("C", 1);

  while (this->DsmManager->GetIsConnected()) {
    if (this->WaitForNotification() > 0) {
      this->DsmManagerInternals->NotificationSocket->Send("N", 1);
      this->WaitForUpdated();
    }
  }
  return((void *)this);
}

//----------------------------------------------------------------------------
void vtkDsmManager::SignalUpdated()
{
#ifdef _WIN32
#if (WINVER >= _WIN32_WINNT_LONGHORN)
  EnterCriticalSection(&this->DsmManagerInternals->UpdatedCritSection);
#endif
#else
  pthread_mutex_lock(&this->DsmManagerInternals->UpdatedMutex);
#endif
  this->DsmManagerInternals->IsUpdated = true;
#ifdef _WIN32
#if (WINVER < _WIN32_WINNT_LONGHORN)
  SetEvent(this->DsmManagerInternals->UpdatedEvent);
#else
  WakeConditionVariable(&this->DsmManagerInternals->UpdatedCond);
  LeaveCriticalSection (&this->DsmManagerInternals->UpdatedCritSection);
#endif
#else
  pthread_cond_signal(&this->DsmManagerInternals->UpdatedCond);
  vtkDebugMacro("Sent updated condition signal");
  pthread_mutex_unlock(&this->DsmManagerInternals->UpdatedMutex);
#endif

  this->DsmManager->ClearNotification();
  this->DsmManager->NotificationFinalize();
}

//----------------------------------------------------------------------------
void vtkDsmManager::WaitForUpdated()
{
#ifdef _WIN32
#if (WINVER >= _WIN32_WINNT_LONGHORN)
  EnterCriticalSection(&this->DsmManagerInternals->UpdatedCritSection);
#endif
#else
  pthread_mutex_lock(&this->DsmManagerInternals->UpdatedMutex);
#endif
  while (!this->DsmManagerInternals->IsUpdated) {
    vtkDebugMacro("Thread going into wait for pipeline updated...");
#ifdef _WIN32
#if (WINVER < _WIN32_WINNT_LONGHORN)
    WaitForSingleObject(this->DsmManagerInternals->UpdatedEvent, INFINITE);
    ResetEvent(this->DsmManagerInternals->UpdatedEvent);
#else
    SleepConditionVariableCS(&this->DsmManagerInternals->UpdatedCond,
        &this->DsmManagerInternals->UpdatedCritSection, INFINITE);
#endif
#else
    pthread_cond_wait(&this->DsmManagerInternals->UpdatedCond,
        &this->DsmManagerInternals->UpdatedMutex);
#endif
    vtkDebugMacro("Thread received updated signal");
  }
  if (this->DsmManagerInternals->IsUpdated) {
    this->DsmManagerInternals->IsUpdated = true;
  }
#ifdef _WIN32
#if (WINVER >= _WIN32_WINNT_LONGHORN)
  LeaveCriticalSection(&this->DsmManagerInternals->UpdatedCritSection);
#endif
#else
  pthread_mutex_unlock(&this->DsmManagerInternals->UpdatedMutex);
#endif
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
        notificationPort++;
      }
    } while (r < 0 && tryConnect < 5);
    if (r < 0) {
      this->DsmManagerInternals->NotificationSocket = NULL;
      return(-1);
    }
  }
  return(this->DsmManager->Create());
}

//----------------------------------------------------------------------------
int vtkDsmManager::Destroy()
{
  return(this->DsmManager->Destroy());
}

//----------------------------------------------------------------------------
int vtkDsmManager::Publish()
{
  this->DsmManager->Publish();
  if (this->UpdatePiece == 0) {
#ifdef _WIN32
  this->DsmManagerInternals->NotificationThreadHandle = CreateThread(NULL, 0,
      vtkDsmManagerNotificationThread, (void *) this, 0,
      &this->DsmManagerInternals->NotificationThreadPtr);
#else
  // Start another thread to handle DSM requests from other nodes
  pthread_create(&this->DsmManagerInternals->NotificationThreadPtr, NULL,
      &vtkDsmManagerNotificationThread, (void *) this);
#endif
  }
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
//  this->ClientServerInterpreter = vtkClientServerInterpreter::New();
//  vtkProcessModule::InitializeInterpreter(DsmProxyHelperInit);
  Initializer->RegisterCallback(&::DsmProxyHelperInit);
  // Pass the DsmProxyHelper XML into the proxy manager for use by NewProxy(...)
//  vtkSMObject::GetProxyManager()->GetProxyDefinitionManager()->LoadConfigurationXMLFromString(xmlstring);
  // Look inside the group name that are tracked
  vtkSMSessionProxyManager* pxm =
      vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager();
  vtkSMProxyDefinitionManager* pxdm = pxm->GetProxyDefinitionManager();

  pxdm->LoadConfigurationXMLFromString(xmlstring);
/////  vtkSMProxyManager::GetProxyManager()->GetProxyDefinitionManager()->LoadConfigurationXMLFromString(xmlstring);
//  vtkSMProxyManager::GetProxyManager()->GetProxyDefinitionManager()->SynchronizeDefinitions();
}

//----------------------------------------------------------------------------
void vtkDsmManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
