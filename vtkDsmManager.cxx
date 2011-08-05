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
  vtkSmartPointer<vtkClientSocket> NotificationSocket;

#ifdef _WIN32
  DWORD  NotificationThreadPtr;
  HANDLE NotificationThreadHandle;
#else
  pthread_t NotificationThreadPtr;
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
  this->XMFDescriptionFilePath  = NULL;
  this->HelperProxyXMLString    = NULL;
  this->DsmManager              = new H5FDdsmManager();
  this->DsmManagerInternals     = new vtkDsmManagerInternals;
}

//----------------------------------------------------------------------------
vtkDsmManager::~vtkDsmManager()
{ 
  this->SetXMFDescriptionFilePath(NULL);
  this->SetHelperProxyXMLString(NULL);
  if (this->DsmManager) delete this->DsmManager;  
  this->DsmManager = NULL;
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
          vtkstd::cout << "MPI_THREAD_MULTIPLE is OK" << vtkstd::endl;
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
//      this->WaitForNotificationFinalize();
    }
  }
  return((void *)this);
}
void vtkDsmManager::NotificationFinalize()
{
  this->DsmManager->NotificationFinalize();
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
  const int notificationPort = 21999;
  if ((this->UpdatePiece == 0) && pvClientHostName && pvClientHostName[0]) {
    vtkstd::cout << "Creating notification channel to "
        << pvClientHostName << " on port " << notificationPort << "...";
    this->DsmManagerInternals->NotificationSocket = vtkSmartPointer<vtkClientSocket>::New();
    int result = this->DsmManagerInternals->NotificationSocket->ConnectToServer(pvClientHostName, notificationPort);
    if (result == 0) {
      vtkstd::cout << "OK" << vtkstd::endl;
    } else {
      vtkErrorMacro("Failed to connect notification socket with client "
          << pvClientHostName << ":" << notificationPort);
      this->DsmManagerInternals->NotificationSocket = NULL;
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
#ifdef _WIN32
  this->DsmManagerInternals->NotificationThreadHandle = CreateThread(NULL, 0,
      vtkDsmManagerNotificationThread, (void *) this, 0,
      &this->DsmManagerInternals->NotificationThreadPtr);
#else
  // Start another thread to handle DSM requests from other nodes
  pthread_create(&this->DsmManagerInternals->NotificationThreadPtr, NULL,
      &vtkDsmManagerNotificationThread, (void *) this);
#endif
  return(1);
}

//----------------------------------------------------------------------------
void vtkDsmManager::GenerateXMFDescription()
{
  XdmfGenerator *xdmfGenerator = new XdmfGenerator();

  if (this->GetDsmBuffer()) {
    xdmfGenerator->SetDsmBuffer(this->GetDsmBuffer());
    xdmfGenerator->Generate((const char*)this->GetXMFDescriptionFilePath(), "DSM:file.h5");
  }
  else {
    xdmfGenerator->Generate((const char*)this->GetXMFDescriptionFilePath(), "file.h5");
  }

  vtkDebugMacro(<< xdmfGenerator->GetGeneratedFile());

  if (this->GetDsmBuffer()) this->GetDsmBuffer()->SetXMLDescription(xdmfGenerator->GetGeneratedFile());
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
  vtkProcessModule::InitializeInterpreter(DsmProxyHelperInit);
  // Pass the DsmProxyHelper XML into the proxy manager for use by NewProxy(...)
  vtkSMObject::GetProxyManager()->LoadConfigurationXML(xmlstring);
}

//----------------------------------------------------------------------------
void vtkDsmManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
