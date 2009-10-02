/*=========================================================================

  Project                 : vtkCSCS
  Module                  : vtkDSMManager.h
  Revision of last commit : $Rev: 793 $
  Author of last commit   : $Author: biddisco $
  Date of last commit     : $Date:: 2009-02-19 12:15:43 +0100 #$

  Copyright (C) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing 
  1) This copyright notice appears on all copies of source code 
  2) An acknowledgment appears with any substantial usage of the code
  3) If this code is contributed to any other open source project, it 
  must not be reformatted such that the indentation, bracketing or 
  overall style is modified significantly. 

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

=========================================================================*/
#include "vtkDSMManager.h"
//
#include "vtkObjectFactory.h"
//
//
#include <vtksys/SystemTools.hxx>
#include <vtksys/RegularExpression.hxx>
#include <vtkstd/vector>
//
#include "vtkSmartPointer.h"

#ifdef VTK_USE_MPI
#include "vtkMPI.h"
#include "vtkMPIController.h"
#include "vtkMPICommunicator.h"
vtkCxxSetObjectMacro(vtkDSMManager, Controller, vtkMultiProcessController);
#endif

#include "XdmfDsmCommMpi.h"
#include "XdmfDsmMsg.h"
#include "XdmfDsmDump.h"

typedef void* (*servicefn)(void *DsmObj) ;
//----------------------------------------------------------------------------
// #ifdef JB_DEBUG__
//----------------------------------------------------------------------------
#ifdef AAWIN32
#define OUTPUTTEXT(a) vtkOutputWindowDisplayText(a);
#else
#define OUTPUTTEXT(a) std::cout << (a) << "\n"; std::cout.flush();
#endif

#undef vtkDebugMacro
#define vtkDebugMacro(a)  \
    { \
  vtkOStreamWrapper::EndlType endl; \
  vtkOStreamWrapper::UseEndl(endl); \
  vtkOStrStreamWrapper vtkmsg; \
  vtkmsg a << "\n"; \
  OUTPUTTEXT(vtkmsg.str()); \
  vtkmsg.rdbuf()->freeze(0); \
    }

#undef vtkErrorMacro
#define vtkErrorMacro(a) vtkDebugMacro(a)
// #endif
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkDSMManager, "$Revision: 793 $");
vtkStandardNewMacro(vtkDSMManager);
//----------------------------------------------------------------------------
vtkDSMManager::vtkDSMManager() 
{
  this->DSMBuffer                = NULL;
  this->LocalBufferSizeMBytes    = 128;
  this->UpdatePiece              = 0;
  this->UpdateNumPieces          = 0;
#ifdef HAVE_PTHREADS
  this->ServiceThread            = 0;
  this->ConnectionThread         = 0;
#elif HAVE_BOOST_THREADS
  this->ServiceThread            = NULL;
  this->ConnectionThread         = NULL;
#endif

  //
#ifdef VTK_USE_MPI
  this->Controller = NULL;
  this->DSMComm    = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
  this->PublishedPortName        = NULL;
  this->AcceptedConnection       = false;
//  this->KillConnection           = false;
#endif
  //
  this->NumberOfTimeSteps        = 0;
  this->TimeStep                 = 0;
  this->FileName                 = NULL;
}
//----------------------------------------------------------------------------
vtkDSMManager::~vtkDSMManager()
{ 
  this->DestroyDSM(); 
  //
  if (this->FileName) {
    delete [] this->FileName;
    this->FileName = NULL;
  }
  if (this->PublishedPortName) {
    delete [] this->PublishedPortName;
    this->PublishedPortName = NULL;
  }

#ifdef VTK_USE_MPI
  this->SetController(NULL);
#endif
}
//----------------------------------------------------------------------------
bool vtkDSMManager::DestroyDSM()
{
  if (this->DSMComm) {
    int rank = this->DSMComm->GetId();
    vtkDebugMacro(<<"DSM destroyed on " << rank);
  }
#ifdef VTK_USE_MPI
  if (this->DSMBuffer && this->UpdatePiece == 0) {
    this->DSMBuffer->SendDone();
  }
#endif
#ifdef HAVE_PTHREADS
  if (this->ServiceThread) {
    pthread_join(this->ServiceThread, NULL);
    this->ServiceThread = 0;
  }
  if (this->ConnectionThread) {
    pthread_join(this->ConnectionThread, NULL);
    this->ConnectionThread = 0;
  }
#elif HAVE_BOOST_THREADS
  if (this->ServiceThread) {
    delete this->ServiceThread;
    this->ServiceThread = NULL;
  }
  if (this->ConnectionThread) {
    delete this->ConnectionThread;
    this->ConnectionThread = NULL;
  }
#endif

  if (this->DSMComm) {
    delete this->DSMComm;
    this->DSMComm = NULL;
  }
  if (this->DSMBuffer) {
    delete this->DSMBuffer;
    this->DSMBuffer = NULL;
  }
  return true;
}
//----------------------------------------------------------------------------
XdmfDsmBuffer *vtkDSMManager::GetDSMHandle()
{
  return DSMBuffer;
}
//----------------------------------------------------------------------------
#ifdef HAVE_PTHREADS
extern "C"{
  void *
  DSMConnectionThread(void *DSMManager){
    vtkDSMManager *manager = (vtkDSMManager *)DSMManager;
    return(manager->AcceptConnection());
  }
}
#elif HAVE_BOOST_THREADS
class DSMServiceThread 
{
public:
  DSMServiceThread(XdmfDsmBuffer *dsmObject)
  {
    this->DSMObject = dsmObject;
  }
  void operator()() {
    //      this->DSMObject->SetGlobalDebug(1);
    this->DSMObject->ServiceThread();
  }
  //
  XdmfDsmBuffer *DSMObject;
};
class DSMConnectionThread
{
public:
  DSMConnectionThread(vtkDSMManager *manager)
  {
    this->DSMManager = manager;
  }
  void operator()() {
    this->DSMManager->AcceptConnection();
  }
  //
  vtkDSMManager *DSMManager;
};
#endif
//----------------------------------------------------------------------------
bool vtkDSMManager::CreateDSM()
{
  if (this->DSMBuffer) {
    return true;
  }

  if (!this->Controller)
    {
    vtkErrorMacro(<<"No MPI Controller specified");
    return false;
    }

#ifdef VTK_USE_MPI
  this->UpdatePiece     = this->Controller->GetLocalProcessId();
  this->UpdateNumPieces = this->Controller->GetNumberOfProcesses();
#else
  this->UpdatePiece     = 0;
  this->UpdateNumPieces = 1;
  vtkErrorMacro(<<"No MPI");
  return 0;
#endif

  //
  // Get the raw MPI_Comm handle
  //
  if (this->Controller->IsA("vtkDummyController")) {
    int provided;
    MPI_Init_thread(0, NULL, MPI_THREAD_MULTIPLE, &provided);
    if (provided != MPI_THREAD_MULTIPLE) {
      vtkstd::cout << "MPI_THREAD_MULTIPLE not set, you may need to recompile your "
      << "MPI distribution with threads enabled" << vtkstd::endl;
    }
    else {
      vtkstd::cout << "MPI_THREAD_MULTIPLE is OK" << vtkstd::endl;
    }
    //      this->SetController();
  }
  vtkMPICommunicator *communicator
  = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator());
  MPI_Comm mpiComm = *communicator->GetMPIComm()->GetHandle();

  //
  // Create Xdmf DSM communicator
  //
  this->DSMComm = new XdmfDsmCommMpi;
  this->DSMComm->DupComm(mpiComm);
  this->DSMComm->Init();

  //
  // Create the DSM buffer
  //
  this->DSMBuffer = new XdmfDsmBuffer();
  // Uniform Dsm : every node has a buffer the same size. (Addresses are sequential)
  this->DSMBuffer->ConfigureUniform(this->DSMComm, this->LocalBufferSizeMBytes*1024*1024);

  // Enable debug
  // this->DSMBuffer->DebugOn();
  // this->DSMBuffer->GetComm()->DebugOn();

  //
  // setup service thread
  //
#ifdef HAVE_PTHREADS
  // Start another thread to handle DSM requests from other nodes
  pthread_create(&this->ServiceThread, NULL, &XdmfDsmBufferServiceThread, (void *) this->DSMBuffer);
#elif HAVE_BOOST_THREADS
  DSMServiceThread MyDSMServiceThread(this->DSMBuffer);
  this->ServiceThread = new boost::thread(MyDSMServiceThread);
#endif

  // Wait for DSM to be ready
  while (!this->DSMBuffer->GetThreadDsmReady()) {
    // Spin until service initialized
  }
  vtkDebugMacro(<<"DSM Service Ready on " << this->UpdatePiece);

  return true;
}

//----------------------------------------------------------------------------
void vtkDSMManager::PublishDSM()
{
  if (this->UpdatePiece == 0) {
    this->DSMBuffer->GetComm()->OpenPort();
    this->SetPublishedPortName(dynamic_cast<XdmfDsmCommMpi*> (this->DSMBuffer->GetComm())->GetDSMPortName());
  }
  //
#ifdef HAVE_PTHREADS
  pthread_create(&this->ConnectionThread, NULL, &DSMConnectionThread, (void *) this);
#elif HAVE_BOOST_THREADS
  DSMConnectionThread MyDSMConnectionThread(this);
  this->ConnectionThread = new boost::thread(MyDSMConnectionThread);
#endif
}
//----------------------------------------------------------------------------
void *vtkDSMManager::AcceptConnection()
{
  this->DSMBuffer->GetComm()->RemoteCommAccept();
  // We are connected and clientComm is set, the Service loop needs now to listen on the clientComm
  this->DSMBuffer->UseRemoteChannel();

  this->Controller->Barrier();
  this->SetAcceptedConnection(true);

  // send DSM information
  XdmfInt64 length = this->DSMBuffer->GetLength();
  XdmfInt64 totalLength = this->DSMBuffer->GetTotalLength();
  XdmfInt32 startServerId = this->DSMBuffer->GetStartServerId();
  XdmfInt32 endServerId = this->DSMBuffer->GetEndServerId();
  this->DSMBuffer->GetComm()->RemoteCommSendInfo(&length, &totalLength, &startServerId, &endServerId);

  return ((void*)this);
}
//----------------------------------------------------------------------------
void vtkDSMManager::UnpublishDSM()
{
  //if (this->AcceptedConnection == false) {
  // Make terminate the thread by forcing the accept
  //MPI_Comm server;
  //this->KillConnection = true;
  //vtkDebugMacro(<<"Connecting for killing connection: " << this->UpdatePiece << " on port: " << this->DSMPortName);
  //MPI_Comm_connect(this->DSMPortName, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &server);
  //vtkDebugMacro(<< "Connected for cleaning!\n");
  //MPI_Comm_disconnect(&server);
  //}

  if(this->UpdatePiece == 0 && this->PublishedPortName != NULL) {
    this->DSMBuffer->GetComm()->ClosePort();
    this->SetPublishedPortName(NULL);
  }
  this->SetAcceptedConnection(false);
  //  this->KillConnection = false;
}
//----------------------------------------------------------------------------
void vtkDSMManager::H5Dump()
{  
  if (this->DSMBuffer) {
    XdmfDsmDump *myDsmDump = new XdmfDsmDump();
    myDsmDump->SetDsmBuffer(this->DSMBuffer);
    if (this->UpdatePiece == 0) {
      myDsmDump->Dump();
    }
    delete myDsmDump;
  }
}
//----------------------------------------------------------------------------
void vtkDSMManager::H5DumpLight()
{  
  if (this->DSMBuffer) {
    XdmfDsmDump *myDsmDump = new XdmfDsmDump();
    myDsmDump->SetDsmBuffer(this->DSMBuffer);
    if (this->UpdatePiece == 0) {
      myDsmDump->DumpLight();
    }
    delete myDsmDump;
  }
}
//----------------------------------------------------------------------------
void vtkDSMManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  //os << indent << "FileName: " <<
  //  (this->FileName ? this->FileName : "(none)") << "\n";

  //os << indent << "NumberOfSteps: " <<  this->NumberOfTimeSteps << "\n";
}
