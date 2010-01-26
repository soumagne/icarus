/*=========================================================================

  Project                 : vtkCSCS
  Module                  : vtkDSMManager.h
  Revision of last commit : $Rev$
  Author of last commit   : $Author$
  Date of last commit     : $Date::                            $

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

#include "XdmfDsmCommSocket.h"
#include "XdmfDsmCommMpi.h"
#include "XdmfDsmMsg.h"
#include "XdmfDsmDump.h"
#include "XdmfGenerator.h"

typedef void* (*servicefn)(void *DsmObj) ;
//----------------------------------------------------------------------------
// #ifdef JB_DEBUG__
//----------------------------------------------------------------------------
#ifdef NO_WIN32
  #define OUTPUTTEXT(a) vtkOutputWindowDisplayText(a);
#else
  #define OUTPUTTEXT(a) std::cout << (a) << std::endl;
#endif

#undef vtkDebugMacro
#define vtkDebugMacro(a)  \
  { \
    vtkOStreamWrapper::EndlType endl; \
    vtkOStreamWrapper::UseEndl(endl); \
    vtkOStrStreamWrapper vtkmsg; \
    vtkmsg a << endl; \
    OUTPUTTEXT(vtkmsg.str()); \
    vtkmsg.rdbuf()->freeze(0); \
  }

#undef vtkErrorMacro
#define vtkErrorMacro(a) vtkDebugMacro(a)
// #endif
//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkDSMManager, "$Revision$");
vtkStandardNewMacro(vtkDSMManager);
//----------------------------------------------------------------------------
vtkDSMManager::vtkDSMManager() 
{
  this->UpdatePiece             = 0;
  this->UpdateNumPieces         = 0;
  this->LocalBufferSizeMBytes   = 128;
#ifdef HAVE_PTHREADS
  this->ServiceThread           = 0;
#elif HAVE_BOOST_THREADS
  this->ServiceThread           = NULL;
#endif

  //
#ifdef VTK_USE_MPI
  this->Controller              = NULL;
  this->SetController(vtkMultiProcessController::GetGlobalController());
  this->DSMBuffer               = NULL;
  this->DSMComm                 = NULL;
  this->DsmCommType             = XDMF_DSM_COMM_MPI;
  this->DsmIsServer             = 1;
  this->PublishedServerHostName = NULL;
  this->PublishedServerPort     = 0;
  this->AcceptedConnection      = 0;
#endif

  this->XMFDescriptionFilePath  = NULL;
  this->DumpDescription         = "";
  this->GeneratedDescription    = "";
}
//----------------------------------------------------------------------------
vtkDSMManager::~vtkDSMManager()
{ 
  this->DestroyDSM();

#ifdef VTK_USE_MPI
  this->SetController(NULL);
#endif
  this->XMFDescriptionFilePath = NULL;
}
//----------------------------------------------------------------------------
int vtkDSMManager::GetAcceptedConnection()
{
  int ret = 0;
  if (this->DSMBuffer) {
    if (this->DSMBuffer->GetIsConnected()) return 1;
  }
  return ret;
}
//----------------------------------------------------------------------------
int vtkDSMManager::GetDsmUpdateReady()
{
  int ret = 0;
  if (this->DSMBuffer) {
    if (this->DSMBuffer->GetIsUpdateReady()) return 1;
  }
  return ret;
}
//----------------------------------------------------------------------------
void vtkDSMManager::ClearDsmUpdateReady()
{
  if (this->DSMBuffer) {
    this->DSMBuffer->SetIsUpdateReady(0);
  }
}
//----------------------------------------------------------------------------
bool vtkDSMManager::DestroyDSM()
{
#ifdef VTK_USE_MPI
  if (this->DSMBuffer && this->UpdatePiece == 0) {
    // @TODO watch out that all processes have empty message queues
    this->DSMBuffer->SendDone();
  }
#endif
#ifdef HAVE_PTHREADS
  if (this->ServiceThread) {
    pthread_join(this->ServiceThread, NULL);
    this->ServiceThread = 0;
  }
#elif HAVE_BOOST_THREADS
  if (this->ServiceThread) {
    this->ServiceThread->join();
    delete this->ServiceThread;
    this->ServiceThread = NULL;
  }
#endif

  if (this->DSMComm) {
    delete this->DSMComm;
    this->DSMComm = NULL;
  }
  if (this->DSMBuffer) {
    delete this->DSMBuffer;
    this->DSMBuffer = NULL;
    vtkDebugMacro(<<"DSM destroyed on " << this->UpdatePiece);
  }

  if (this->PublishedServerHostName) this->PublishedServerHostName = NULL;

  return true;
}
//----------------------------------------------------------------------------
XdmfDsmBuffer *vtkDSMManager::GetDSMHandle()
{
  return DSMBuffer;
}
//----------------------------------------------------------------------------
#ifdef HAVE_PTHREADS
// nothing at the moment
#elif HAVE_BOOST_THREADS
class DSMServiceThread 
{
public:
  DSMServiceThread(XdmfDsmBuffer *dsmObject)
  {
    this->DSMObject = dsmObject;
  }
  void operator()() {
    this->DSMObject->ServiceThread();
  }
  //
  XdmfDsmBuffer *DSMObject;
};
#endif
//----------------------------------------------------------------------------
bool vtkDSMManager::CreateDSM()
{
  if (this->DSMBuffer) {
    return true;
  }

  if (this->Controller->IsA("vtkDummyController"))
  {
    int flag = 0;
    MPI_Initialized(&flag);
    if (flag == 0)
    {
      vtkErrorMacro(<<"Running without MPI, attempting to initialize ");
      int argc = 1;
      const char *argv = "D:\\cmakebuild\\pv-shared\\bin\\RelWithDebInfo\\paraview.exe";
      char **_argv = (char**) &argv;
      int provided, rank, size;
      MPI_Init_thread(&argc, &_argv, MPI_THREAD_MULTIPLE, &provided);
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
      vtkSmartPointer<vtkMPIController> controller = vtkSmartPointer<vtkMPIController>::New();
      controller->Initialize(&argc, &_argv, 1);
      this->SetController(controller);
      vtkMPIController::SetGlobalController(controller);
    }

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
  vtkMPICommunicator *communicator
  = vtkMPICommunicator::SafeDownCast(this->Controller->GetCommunicator());
  MPI_Comm mpiComm = *communicator->GetMPIComm()->GetHandle();

  //
  // Create Xdmf DSM communicator
  //
  if (this->GetDsmCommType() == XDMF_DSM_COMM_MPI) {
    this->DSMComm = new XdmfDsmCommMpi();
    vtkDebugMacro(<< "Using MPI Intercomm...");
    dynamic_cast<XdmfDsmCommMpi*> (this->DSMComm)->DupComm(mpiComm);
  }
  else if (this->GetDsmCommType() == XDMF_DSM_COMM_SOCKET) {
    this->DSMComm = new XdmfDsmCommSocket();
    vtkDebugMacro(<< "Using Socket Intercomm...");
    dynamic_cast<XdmfDsmCommSocket*> (this->DSMComm)->DupComm(mpiComm);
  }
  this->DSMComm->DebugOn();
  this->DSMComm->Init();
  //
  // Create the DSM buffer
  //
  this->DSMBuffer = new XdmfDsmBuffer();
  this->DSMBuffer->DebugOn();
  this->DSMBuffer->SetServiceThreadUseCopy(0);
  // Uniform Dsm : every node has a buffer the same size. (Addresses are sequential)
  this->DSMBuffer->ConfigureUniform(this->DSMComm, this->GetLocalBufferSizeMBytes()*1024*1024);

  //
  // setup service thread
  //
  if (this->DsmIsServer == 1) {
    vtkDebugMacro(<< "Creating service thread...");
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
    this->DSMBuffer->SetIsServer(true);
    vtkDebugMacro(<<"DSM Service Ready on " << this->UpdatePiece);
  } else {
    this->DSMBuffer->SetIsServer(false);
  }

  return true;
}
//----------------------------------------------------------------------------
void vtkDSMManager::ClearDSM()
{
  this->DSMBuffer->ClearStorage();
  if (this->UpdatePiece == 0) vtkDebugMacro(<< "DSM cleared");
}
//----------------------------------------------------------------------------
void vtkDSMManager::ConnectDSM()
{
  if (this->UpdatePiece == 0) vtkDebugMacro(<< "Connect DSM");

  if (this->GetDsmCommType() == XDMF_DSM_COMM_MPI) {
    if (this->GetPublishedServerHostName() != NULL) {
      dynamic_cast<XdmfDsmCommMpi*> (this->DSMBuffer->GetComm())->SetDsmMasterHostName(this->GetPublishedServerHostName());
      if (this->UpdatePiece == 0) {
        vtkDebugMacro(<< "Initializing connection to "
            << dynamic_cast<XdmfDsmCommMpi*> (this->DSMBuffer->GetComm())->GetDsmMasterHostName());
      }
    }
  }
  else if (this->GetDsmCommType() == XDMF_DSM_COMM_SOCKET) {
    if ((this->GetPublishedServerHostName() != NULL) && (this->GetPublishedServerPort() != 0)) {
      dynamic_cast<XdmfDsmCommSocket*> (this->DSMBuffer->GetComm())->SetDsmMasterHostName(this->GetPublishedServerHostName());
      dynamic_cast<XdmfDsmCommSocket*> (this->DSMBuffer->GetComm())->SetDsmMasterPort(this->GetPublishedServerPort());
      if (this->UpdatePiece == 0) {
        vtkDebugMacro(<< "Initializing connection to "
            << dynamic_cast<XdmfDsmCommSocket*> (this->DSMBuffer->GetComm())->GetDsmMasterHostName()
            << ":"
            << dynamic_cast<XdmfDsmCommSocket*> (this->DSMBuffer->GetComm())->GetDsmMasterPort());
      }
    }
  }
  else {
    if (this->UpdatePiece == 0) vtkErrorMacro(<< "NULL port");
  }
}
//----------------------------------------------------------------------------
void vtkDSMManager::DisconnectDSM()
{
  if (this->UpdatePiece == 0) vtkDebugMacro(<< "Disconnect DSM");
  this->DSMBuffer->RequestDisconnection(); // Go back to normal channel
}
//----------------------------------------------------------------------------
void vtkDSMManager::PublishDSM()
{
  if (this->UpdatePiece == 0) vtkDebugMacro(<< "Opening port...");
  this->DSMBuffer->GetComm()->OpenPort();

  if (this->UpdatePiece == 0) {
    if (this->GetDsmCommType() == XDMF_DSM_COMM_MPI) {
      this->SetPublishedServerHostName(dynamic_cast<XdmfDsmCommMpi*>
         (this->DSMBuffer->GetComm())->GetDsmMasterHostName());
         vtkDebugMacro(<< "Server PortName: " << this->GetPublishedServerHostName());
    }
    else if (this->GetDsmCommType() == XDMF_DSM_COMM_SOCKET) {
      this->SetPublishedServerHostName(dynamic_cast<XdmfDsmCommSocket*>
      (this->DSMBuffer->GetComm())->GetDsmMasterHostName());
      this->SetPublishedServerPort(dynamic_cast<XdmfDsmCommSocket*>
      (this->DSMBuffer->GetComm())->GetDsmMasterPort());
      vtkDebugMacro(<< "Server HostName: " << this->GetPublishedServerHostName()
          << ", Server port: " << this->GetPublishedServerPort());
    }
  }
  //
  this->DSMBuffer->RequestRemoteChannel();
}
//----------------------------------------------------------------------------

void vtkDSMManager::UnpublishDSM()
{
  //if (this->AcceptedConnection == false) {
  // Make terminate the thread by forcing the accept
  // TODO Should simply add a signal to the socket
  //}
  if (this->GetPublishedServerHostName() != NULL) {
    this->DSMBuffer->GetComm()->ClosePort();

    if (this->UpdatePiece == 0) {
      this->SetPublishedServerHostName(NULL);
      this->SetPublishedServerPort(0);
    }
  }
  this->SetAcceptedConnection(0);
}
//----------------------------------------------------------------------------
void vtkDSMManager::H5Dump()
{  
  if (this->DSMBuffer) {
    XdmfDsmDump *myDsmDump = new XdmfDsmDump();
    myDsmDump->SetDsmBuffer(this->DSMBuffer);
    myDsmDump->SetFileName("DSM.h5");
    myDsmDump->Dump();
    if (this->UpdatePiece == 0) vtkDebugMacro(<< "Dump done");
    delete myDsmDump;
  }
}
//----------------------------------------------------------------------------
void vtkDSMManager::H5DumpLight()
{  
  if (this->DSMBuffer) {
    XdmfDsmDump *myDsmDump = new XdmfDsmDump();
    myDsmDump->SetDsmBuffer(this->DSMBuffer);
    myDsmDump->SetFileName("DSM.h5");
    myDsmDump->DumpLight();
    if (this->UpdatePiece == 0) vtkDebugMacro(<< "Dump light done");
    delete myDsmDump;
  }
}
//----------------------------------------------------------------------------
void vtkDSMManager::H5DumpXML()
{
  if (this->DSMBuffer) {
    std::ostringstream dumpStream;
    XdmfDsmDump *myDsmDump = new XdmfDsmDump();
    myDsmDump->SetDsmBuffer(this->DSMBuffer);
    myDsmDump->SetFileName("DSM.h5");
    myDsmDump->DumpXML(dumpStream);
    if (this->UpdatePiece == 0) vtkDebugMacro(<< "Dump XML done");
    this->DumpDescription = dumpStream.str();
    //if (this->UpdatePiece == 0) vtkDebugMacro(<< this->DumpDescription.c_str());
    delete myDsmDump;
  }
}
//----------------------------------------------------------------------------
void vtkDSMManager::GenerateXMFDescription()
{
  XdmfGenerator *xdmfGenerator = new XdmfGenerator();

  if (this->DSMBuffer) {
    xdmfGenerator->SetHdfFileName("DSM:DSM.h5");
  }
  else {
    xdmfGenerator->SetHdfFileName("DSM.h5");
  }
  xdmfGenerator->GenerateHead();
  xdmfGenerator->Generate((const char*)this->GetXMFDescriptionFilePath(), this->DumpDescription.c_str());
  this->GeneratedDescription = xdmfGenerator->GetGeneratedFile();
  if (this->UpdatePiece == 0) vtkDebugMacro(<< this->GeneratedDescription.c_str());
  delete xdmfGenerator;
}
//----------------------------------------------------------------------------
void vtkDSMManager::SendDSMXML()
{
  if (this->GetXMFDescriptionFilePath() != NULL) {
    this->DSMBuffer->RequestXMLExchange();
    this->DSMBuffer->GetComm()->RemoteCommSendXML(this->GetXMFDescriptionFilePath());
  }
}
//----------------------------------------------------------------------------
void vtkDSMManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
