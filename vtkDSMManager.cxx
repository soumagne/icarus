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

//----------------------------------------------------------------------------
#define JB_DEBUG__
#ifdef JB_DEBUG__
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
#endif
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
  this->ServerHostName          = NULL;
  this->ServerPort              = 0;
#endif

  this->XMFDescriptionFilePath  = NULL;
  this->XMLStringSend           = NULL;
  this->DumpDescription         = NULL;
}
//----------------------------------------------------------------------------
vtkDSMManager::~vtkDSMManager()
{ 
  this->DestroyDSM();

#ifdef VTK_USE_MPI
  this->SetController(NULL);
#endif
  this->XMFDescriptionFilePath = NULL;
  if (this->XMLStringSend) {
    delete []this->XMLStringSend;
  }
  this->XMLStringSend = NULL;

  if (this->DumpDescription) {
    delete []this->DumpDescription;
  }
  this->DumpDescription = NULL;
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

  if (this->ServerHostName) this->ServerHostName = NULL;

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
    vtkErrorMacro(<<"Running vtkDummyController : replacing it");
    int flag = 0;
    MPI_Initialized(&flag);
    if (flag == 0)
    {
      vtkErrorMacro(<<"Running without MPI, attempting to initialize ");
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
    vtkErrorMacro(<<"Setting Global MPI controller");
    vtkSmartPointer<vtkMPIController> controller = vtkSmartPointer<vtkMPIController>::New();
    if (flag == 0) controller->Initialize(NULL, NULL, 1);
    this->SetController(controller);
    vtkMPIController::SetGlobalController(controller);
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
  // this->DSMComm->DebugOn();
  this->DSMComm->Init();
  //
  // Create the DSM buffer
  //
  this->DSMBuffer = new XdmfDsmBuffer();
  // this->DSMBuffer->DebugOn();
  this->DSMBuffer->SetServiceThreadUseCopy(0);
  // Uniform Dsm : every node has a buffer the same size. (Addresses are sequential)
  this->DSMBuffer->ConfigureUniform(this->DSMComm, ((long long) this->GetLocalBufferSizeMBytes())*1024*1024);
  if (this->UpdatePiece == 0) {
    vtkDebugMacro(<< "Length set: " << this->DSMBuffer->GetLength() <<
       ", totalLength set: " << this->DSMBuffer->GetTotalLength() <<
       ", startServerId set: " << this->DSMBuffer->GetStartServerId() <<
       ", endServerId set: " << this->DSMBuffer->GetEndServerId());
  }
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
void vtkDSMManager::RequestRemoteChannel()
{
  this->DSMBuffer->RequestRemoteChannel();
}
//----------------------------------------------------------------------------
void vtkDSMManager::ConnectDSM()
{
  if (this->UpdatePiece == 0) vtkDebugMacro(<< "Connect DSM");

  if (this->GetDsmCommType() == XDMF_DSM_COMM_MPI) {
    if (this->GetServerHostName() != NULL) {
      dynamic_cast<XdmfDsmCommMpi*> (this->DSMBuffer->GetComm())->SetDsmMasterHostName(this->GetServerHostName());
      if (this->UpdatePiece == 0) {
        vtkDebugMacro(<< "Initializing connection to "
            << dynamic_cast<XdmfDsmCommMpi*> (this->DSMBuffer->GetComm())->GetDsmMasterHostName());
      }
    }
  }
  else if (this->GetDsmCommType() == XDMF_DSM_COMM_SOCKET) {
    if ((this->GetServerHostName() != NULL) && (this->GetServerPort() != 0)) {
      dynamic_cast<XdmfDsmCommSocket*> (this->DSMBuffer->GetComm())->SetDsmMasterHostName(this->GetServerHostName());
      dynamic_cast<XdmfDsmCommSocket*> (this->DSMBuffer->GetComm())->SetDsmMasterPort(this->GetServerPort());
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
  if (this->GetDsmCommType() == XDMF_DSM_COMM_SOCKET) {
    dynamic_cast<XdmfDsmCommSocket*>
    (this->DSMBuffer->GetComm())->SetDsmMasterHostName(this->GetServerHostName());
    dynamic_cast<XdmfDsmCommSocket*>
    (this->DSMBuffer->GetComm())->SetDsmMasterPort(this->GetServerPort());
  }
  this->DSMBuffer->GetComm()->OpenPort();

  if (this->UpdatePiece == 0) {
    if (this->GetDsmCommType() == XDMF_DSM_COMM_MPI) {
      this->SetServerHostName(dynamic_cast<XdmfDsmCommMpi*>
         (this->DSMBuffer->GetComm())->GetDsmMasterHostName());
         vtkDebugMacro(<< "Server PortName: " << this->GetServerHostName());
    }
    else if (this->GetDsmCommType() == XDMF_DSM_COMM_SOCKET) {
      this->SetServerHostName(dynamic_cast<XdmfDsmCommSocket*>
      (this->DSMBuffer->GetComm())->GetDsmMasterHostName());
      this->SetServerPort(dynamic_cast<XdmfDsmCommSocket*>
      (this->DSMBuffer->GetComm())->GetDsmMasterPort());
      vtkDebugMacro(<< "Server HostName: " << this->GetServerHostName()
          << ", Server port: " << this->GetServerPort());
    }
  }
  //
  this->DSMBuffer->RequestRemoteChannel();
}
//----------------------------------------------------------------------------

void vtkDSMManager::UnpublishDSM()
{
  if (this->UpdatePiece == 0) vtkDebugMacro(<< "Closing port...");
  this->DSMBuffer->GetComm()->ClosePort();

  if (this->UpdatePiece == 0) {
    if (this->GetServerHostName() != NULL) {
      this->SetServerHostName(NULL);
      this->SetServerPort(0);
    }
  }
  if (this->UpdatePiece == 0) vtkDebugMacro(<< "Port closed");
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
    int dumpDescLength;
    std::ostringstream dumpStream;
    XdmfDsmDump *myDsmDump = NULL;

    if (this->UpdatePiece == 0) {
      myDsmDump = new XdmfDsmDump();
      myDsmDump->SetDsmBuffer(this->DSMBuffer);
      myDsmDump->SetFileName("DSM.h5");
      myDsmDump->DumpXML(dumpStream);
      vtkDebugMacro(<< "Dump XML done");
      dumpDescLength = dumpStream.str().length();
    }

    this->Controller->Broadcast(&dumpDescLength, 1, 0);
    if (this->DumpDescription) delete []this->DumpDescription;
    this->DumpDescription = new char[dumpDescLength + 1];
    if (this->UpdatePiece == 0) strcpy(this->DumpDescription, dumpStream.str().c_str());
    this->Controller->Broadcast(this->DumpDescription, dumpDescLength + 1, 0);

    vtkDebugMacro(<< this->DumpDescription);

    if (this->UpdatePiece == 0) delete myDsmDump;
  }
}
//----------------------------------------------------------------------------
void vtkDSMManager::GenerateXMFDescription()
{
  XdmfGenerator *xdmfGenerator = new XdmfGenerator();

  if (this->DSMBuffer) {
    xdmfGenerator->SetHdfFileName("DSM:file.h5");
  }
  else {
    xdmfGenerator->SetHdfFileName("file.h5");
  }
  xdmfGenerator->GenerateHead();
  xdmfGenerator->Generate((const char*)this->GetXMFDescriptionFilePath(), this->DumpDescription);

  vtkDebugMacro(<< xdmfGenerator->GetGeneratedFile());

  if (this->DSMBuffer) this->DSMBuffer->SetXMLDescription(xdmfGenerator->GetGeneratedFile());
  delete xdmfGenerator;
}
//----------------------------------------------------------------------------
void vtkDSMManager::SendDSMXML()
{
  this->DSMBuffer->RequestXMLExchange();
  if (this->XMLStringSend != NULL) {
    int commServerSize = this->DSMBuffer->GetEndServerId() - this->DSMBuffer->GetStartServerId() + 1;
    for (int i=0; i<commServerSize; i++) {
      this->DSMBuffer->GetComm()->RemoteCommSendXML(this->XMLStringSend, i);
    }
  }
  this->DSMBuffer->GetComm()->Barrier();
}
//----------------------------------------------------------------------------
const char *vtkDSMManager::GetXMLStringReceive()
{
  if (this->DSMBuffer) return this->DSMBuffer->GetXMLDescription();
  return NULL;
}
//----------------------------------------------------------------------------
void vtkDSMManager::ClearXMLStringReceive()
{
  this->DSMBuffer->SetXMLDescription(NULL);
}
//----------------------------------------------------------------------------
void vtkDSMManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
