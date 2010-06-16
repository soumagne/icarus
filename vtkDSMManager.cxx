/*=========================================================================

  Project                 : vtkCSCS
  Module                  : vtkDSMManager.h

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
//
#include "H5FDdsmCommSocket.h"
#include "H5FDdsmCommMpi.h"
#include "H5FDdsmMsg.h"
//
#include "XdmfDump.h"
#include "XdmfGenerator.h"

//----------------------------------------------------------------------------
#undef  vtkDebugMacro
#define vtkDebugMacro(a) H5FDdsmExternalDebug(a)

#undef  vtkErrorMacro
#define vtkErrorMacro(a) H5FDdsmExternalError(a)
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
  this->DsmCommType             = H5FD_DSM_COMM_MPI;
  this->DsmIsServer             = 1;
  this->ServerHostName          = NULL;
  this->ServerPort              = 0;
#endif

  this->XMFDescriptionFilePath  = NULL;
  this->XMLStringSend           = NULL;
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
H5FDdsmBuffer *vtkDSMManager::GetDSMHandle()
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
  DSMServiceThread(H5FDdsmBuffer *dsmObject)
  {
    this->DSMObject = dsmObject;
  }
  void operator()() {
    this->DSMObject->ServiceThread();
  }
  //
  H5FDdsmBuffer *DSMObject;
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
  if (this->GetDsmCommType() == H5FD_DSM_COMM_MPI) {
    this->DSMComm = new H5FDdsmCommMpi();
    vtkDebugMacro(<< "Using MPI Intercomm...");
    dynamic_cast<H5FDdsmCommMpi*> (this->DSMComm)->DupComm(mpiComm);
  }
  else if (this->GetDsmCommType() == H5FD_DSM_COMM_SOCKET) {
    this->DSMComm = new H5FDdsmCommSocket();
    vtkDebugMacro(<< "Using Socket Intercomm...");
    dynamic_cast<H5FDdsmCommSocket*> (this->DSMComm)->DupComm(mpiComm);
  }
  // this->DSMComm->DebugOn();
  this->DSMComm->Init();
  //
  // Create the DSM buffer
  //
  this->DSMBuffer = new H5FDdsmBuffer();
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
    pthread_create(&this->ServiceThread, NULL, &H5FDdsmBufferServiceThread, (void *) this->DSMBuffer);
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

  if (this->GetDsmCommType() == H5FD_DSM_COMM_MPI) {
    if (this->GetServerHostName() != NULL) {
      dynamic_cast<H5FDdsmCommMpi*> (this->DSMBuffer->GetComm())->SetDsmMasterHostName(this->GetServerHostName());
      if (this->UpdatePiece == 0) {
        vtkDebugMacro(<< "Initializing connection to "
            << dynamic_cast<H5FDdsmCommMpi*> (this->DSMBuffer->GetComm())->GetDsmMasterHostName());
      }
    }
  }
  else if (this->GetDsmCommType() == H5FD_DSM_COMM_SOCKET) {
    if ((this->GetServerHostName() != NULL) && (this->GetServerPort() != 0)) {
      dynamic_cast<H5FDdsmCommSocket*> (this->DSMBuffer->GetComm())->SetDsmMasterHostName(this->GetServerHostName());
      dynamic_cast<H5FDdsmCommSocket*> (this->DSMBuffer->GetComm())->SetDsmMasterPort(this->GetServerPort());
      if (this->UpdatePiece == 0) {
        vtkDebugMacro(<< "Initializing connection to "
            << dynamic_cast<H5FDdsmCommSocket*> (this->DSMBuffer->GetComm())->GetDsmMasterHostName()
            << ":"
            << dynamic_cast<H5FDdsmCommSocket*> (this->DSMBuffer->GetComm())->GetDsmMasterPort());
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
  if (this->GetDsmCommType() == H5FD_DSM_COMM_SOCKET) {
    dynamic_cast<H5FDdsmCommSocket*>
    (this->DSMBuffer->GetComm())->SetDsmMasterHostName(this->GetServerHostName());
    dynamic_cast<H5FDdsmCommSocket*>
    (this->DSMBuffer->GetComm())->SetDsmMasterPort(this->GetServerPort());
  }
  this->DSMBuffer->GetComm()->OpenPort();

  if (this->UpdatePiece == 0) {
    H5FDdsmIniFile dsmConfigFile;
    std::string fullDsmConfigFilePath;
    const char *dsmEnvPath = getenv("DSM_CONFIG_PATH");
    if (dsmEnvPath) {
      fullDsmConfigFilePath = std::string(dsmEnvPath) + std::string("/.dsm_config");
      dsmConfigFile.Create(fullDsmConfigFilePath);
      dsmConfigFile.AddSection("Comm", fullDsmConfigFilePath);
      std::cout << "Written " << fullDsmConfigFilePath.c_str() << std::endl;
    }
    if (this->GetDsmCommType() == H5FD_DSM_COMM_MPI) {
      this->SetServerHostName(dynamic_cast<H5FDdsmCommMpi*>
      (this->DSMBuffer->GetComm())->GetDsmMasterHostName());
      vtkDebugMacro(<< "Server PortName: " << this->GetServerHostName());
      if (dsmEnvPath) {
        dsmConfigFile.SetValue("DSM_COMM_SYSTEM", "mpi", "Comm", fullDsmConfigFilePath);
        dsmConfigFile.SetValue("DSM_BASE_HOST", this->GetServerHostName(), "Comm", fullDsmConfigFilePath);
      }
    }
    else if (this->GetDsmCommType() == H5FD_DSM_COMM_SOCKET) {
      this->SetServerHostName(dynamic_cast<H5FDdsmCommSocket*>
      (this->DSMBuffer->GetComm())->GetDsmMasterHostName());
      this->SetServerPort(dynamic_cast<H5FDdsmCommSocket*>
      (this->DSMBuffer->GetComm())->GetDsmMasterPort());
      vtkDebugMacro(<< "Server HostName: " << this->GetServerHostName()
          << ", Server port: " << this->GetServerPort());
      if (dsmEnvPath) {
        char serverPort[32];
        sprintf(serverPort, "%d", this->GetServerPort());
        dsmConfigFile.SetValue("DSM_COMM_SYSTEM", "socket", "Comm", fullDsmConfigFilePath);
        dsmConfigFile.SetValue("DSM_BASE_HOST", this->GetServerHostName(), "Comm", fullDsmConfigFilePath);
        dsmConfigFile.SetValue("DSM_BASE_PORT", serverPort, "Comm", fullDsmConfigFilePath);
      }
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
    XdmfDump *myDsmDump = new XdmfDump();
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
    XdmfDump *myDsmDump = new XdmfDump();
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
    XdmfDump *myDsmDump = new XdmfDump();
    myDsmDump->SetDsmBuffer(this->DSMBuffer);
    myDsmDump->SetFileName("DSM.h5");
    myDsmDump->DumpXML(dumpStream);
    if (this->UpdatePiece == 0) vtkDebugMacro(<< "Dump XML done");
    if (this->UpdatePiece == 0) vtkDebugMacro(<< dumpStream.str().c_str());
    delete myDsmDump;
  }
}
//----------------------------------------------------------------------------
void vtkDSMManager::GenerateXMFDescription()
{
  XdmfGenerator *xdmfGenerator = new XdmfGenerator();

  if (this->DSMBuffer) {
    xdmfGenerator->SetDsmBuffer(this->DSMBuffer);
    xdmfGenerator->Generate((const char*)this->GetXMFDescriptionFilePath(), "DSM:file.h5");
  }
  else {
    xdmfGenerator->Generate((const char*)this->GetXMFDescriptionFilePath(), "file.h5");
  }

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
bool vtkDSMManager::ReadDSMConfigFile()
{
  H5FDdsmIniFile config;
  std::string configPath;
  const char *dsm_env = getenv("H5FD_DSM_CONFIG_PATH");
  if (dsm_env) {
    configPath = std::string(dsm_env) + std::string("/.dsm_config");
  }
  if (vtksys::SystemTools::FileExists(configPath.c_str())) {
    std::string mode = config.GetValue("DSM_COMM_SYSTEM", "Comm", configPath);
    std::string host = config.GetValue("DSM_BASE_HOST",   "Comm", configPath);
    std::string port = config.GetValue("DSM_BASE_PORT",   "Comm", configPath);
    if (mode == "socket") {
      this->SetDsmCommType(H5FD_DSM_COMM_SOCKET);
      this->SetServerPort(atoi(port.c_str()));
    } else if (mode == "mpi") {
      this->SetDsmCommType(H5FD_DSM_COMM_MPI);
    }
    this->SetServerHostName(host.c_str());
    return true;
  }
  return false;
}
//----------------------------------------------------------------------------
void vtkDSMManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
