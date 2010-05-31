// -D D:/data/cfxtestdata/HSLU -F Vers1_Variable_001.res -O D:/data/xdmf/test
// -D D:/data/cfxtestdata/HSLU -F Vers1_Variable_001.res -O D:/data/xdmf/scratch -I 0.002
// -D D:\data\mhd -F s15t_00000_0000-00.xdmf -O D:/data/xdmf/scratch
// -D D:/data/VectorField -F damBreakMarin.pvd -O D:/data/xdmf/scratch
// -D D:/data/cfxtestdata/HSLU -F Vers1_Variable_001.res -O D:/data/xdmf/scratch
// -D D:/data -F Test_UG_hex_elev.vtu -O D:/data/xdmf/scratch -v
// -D D:/data -F one_cell.vtu -O D:/data/xdmf/scratch -v
// -D H:/paraviewdata/data -F Elements.vtu -O D:/data/xdmf/scratch -v
// -D D:\data\cfx\SmallExample -F TRS_Default_004.res -O D:/data/xdmf/scratch
//
// jfavre data
// -D C:\data\jwalther\case004.dtvis_small -F H_helI00000.pvti -O D:/data/xdmf/scratch 
// -D C:\data\jwalther\ring -F ringRe25k_omegaI02000.pvti -O C:/data/xdmf/scratch 
//
// For Testing DSM
// Machine a=dino) -D C:\data\xdmf\other -F cav_DSM_Test.xmf -Client
// Machine b=agno) -Server

//----------------------------------------------------------------------------
#include "vtkXdmfWriter4.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include "vtkPointData.h"
#include "vtkCellData.h"
#include "vtkRenderer.h"
#include "vtkUnstructuredGrid.h"
#include "vtkCompositeDataIterator.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkCompositeDataPipeline.h"
#include "vtkInformation.h"
#include "vtkCompositePolyDataMapper2.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTemporalDataSetCache.h"
#include "vtkTemporalInterpolator.h"
#include "vtkTemporalDataSet.h"
#include "vtkExtentTranslator.h"

// vtk MPI
#include "vtkMPIController.h"
#include "vtkParallelFactory.h"

// vtk Testing 
#include "vtkTesting.h"
#include "Testing/Cxx/vtkTestUtilities.h"
#include <vtksys/SystemTools.hxx>

// Readers we support
#include "vtkXMLUnstructuredGridReader.h"
#include "vtkXMLMultiBlockDataReader.h"
#include "vtkXMLCollectionReader.h"
#include "vtkXMLPImageDataReader.h"
#include "vtkPVDReader.h"
#include "vtkXdmfReader.h"
#include "vtkXdmfReader3.h"
#include "vtkXdmfReader4.h"

#ifndef WIN32
  #define HAVE_PTHREADS
  extern "C" {
    #include <pthread.h>
  }
#elif HAVE_BOOST_THREADS
  #include <boost/thread/thread.hpp> // Boost Threads
#endif

// CSCS
//#include "vtkStreamOutputWindow.h"

// Xdmf/DSM features 
#include "H5FDdsm.h"
#include "vtkDSMManager.h"

// Sys
#include <sstream>
#include <mpi.h>

#define PARALLEL_PIECES

#ifdef HAVE_PTHREADS
    pthread_t      ServiceThread;
#elif HAVE_BOOST_THREADS
    boost::thread *ServiceThread;
#endif

typedef std::vector<double>::size_type itype;

#ifdef MACHINE_AGNO
  #define server "agno.staff.cscs.ch"
  #define client "agno.staff.cscs.ch"
  #define PORT 22000
#endif

#ifdef MACHINE_DINO
  #define server "agno.staff.cscs.ch"
  #define client "dino.staff.cscs.ch"
  #define PORT 22000
#endif

#ifdef MACHINE_BRENO
  #define server "agno.staff.cscs.ch"
  #define client "breno.staff.cscs.ch"
  #define PORT 22000
#endif

#ifndef server
  #define server "agno.staff.cscs.ch"
  #define client "breno.staff.cscs.ch"
  #define PORT 22000
#endif

std::string server_name = server;
std::string client_name = client;
int default_port_number = PORT;

//----------------------------------------------------------------------------
std::string usage = "\n"\
    "Usage : ConvertToXdmf \n" \
    " Win32  : ConvertToXdmf -D D:/data/xdmf -F elements.vtu -O D:/data/xdmf/test \n" \
    " Win32  : ConvertToXdmf -D D:/data/VectorField -F damBreakMarin.pvd -O D:/data/xdmf/test \n" \
    " Win32  : ConvertToXdmf -D D:/data/cfxtestdata/Example -F TRS_Default_003.res -O D:/data/xdmf/test \n" \
    " Win32  : ConvertToXdmf -D C:/data/xdmf/test -F TRS_Default_003.xmf -O C:/data/xdmf/scratch \n" \
    " laptop : ConvertToXdmf -D C:\\Data\\cfxtestdata\\Example -F TRS_Default_003.res -O C:\\Data\\cfxtestdata\\xml \n" \
    " Linux  : ConvertToXdmf -r -b -s -D /project/csvis/CFXData/Francis -F 1800_001.res -O /project/csvis/CFXData/XML \n" \
    "  -D /project/csvis/CFXData/Francis \n"        \
    "  -F 1800_001.res \n"                          \
    "  -O output directory \n"                      \
    "  -i timestep - interpolate data using timestep between outputs \n" \
    "  -r rotate vectors for stn frame \n"          \
    "  -f flip 180degrees (csf data) \n"            \
    "  -a append to existing set (otherwise overwrite) \n" \
    "  -s Force Static flag \n" \
    "  -v Subdivide cells (only Tetrahedra + Hexahedra supported currently) \n" \
    "  -Client Send data from DSM Client to DSM Server \n" \
    "  -Server Wait for data from DSM Client \n" \
    "  -DSM Write to self contained DSM \n";
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void ThreadExecute(void *dsm, vtkTypeInt64 &counter) {
  vtkDSMManager *DSM = (vtkDSMManager*)dsm;
  if (DSM->GetDsmUpdateReady()) {
    DSM->H5DumpLight();
    DSM->ClearDsmUpdateReady();
    counter ++;
  }
};
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
#ifdef HAVE_PTHREADS
  // nothing required
#elif HAVE_BOOST_THREADS
class DSMListenThread {
  public:
    DSMListenThread(vtkDSMManager *dsm)
    {
      this->DSMManager = dsm;
      Counter          = 0;
      UpdatesCounter   = 0;
    }
    void operator()() {
      while (this->DSMManager) {
        UpdatesCounter ++;
        ThreadExecute(this->DSMManager, Counter);
        std::cout << UpdatesCounter << " : " << Counter << std::endl;
        // somed delay here ?
      }
    }
    //
    vtkDSMManager *DSMManager;
    H5FDdsmInt64   Counter;
    H5FDdsmInt64   UpdatesCounter;
};
#endif
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
struct ParallelArgs_tmp {
  int* retVal;
  int    argc;
  char** argv;
};
//----------------------------------------------------------------------------
// This will be called by all processes
void MyMain( vtkMultiProcessController *controller, void *arg )
{
  // Obtain the id of the running process and the total
  // number of processes
  vtkTypeInt64 myId = controller->GetLocalProcessId();
  vtkTypeInt64 numProcs = controller->GetNumberOfProcesses();

  std::cout << "Process number " << myId << " of " << numProcs << std::endl;

  //
  // begin scope block so auto-deletion of memory can be checked
  //
  { 
    //
    // Use utilities in vtkTesting for getting command line params
    //
    ParallelArgs_tmp* args = reinterpret_cast<ParallelArgs_tmp*>(arg);
    vtkSmartPointer<vtkTesting> test = vtkSmartPointer<vtkTesting>::New();
    for (int c=1; c<args->argc; c++ ) {
      test->AddArgument(args->argv[c]);
    }
    //
    //bool DSMserver = true;

    //
    // Start of pipeline (unused now)
    //
    vtkSmartPointer<vtkAlgorithm> algorithm;
    algorithm = NULL;

    vtkSmartPointer<vtkCompositeDataPipeline> defaultexecutive = 
      vtkSmartPointer<vtkCompositeDataPipeline>::New();
    vtkAlgorithm::SetDefaultExecutivePrototype(defaultexecutive);      

    //
    // Set Update piece information so pipeline can split correctly
    // actually, it's invalid to do it here, but we're testing
    //
    vtkSmartPointer<vtkInformation> execInfo;

    //
    // if we are running a parallel job, sync here
    //
    controller->Barrier();

    //
    // If set, setup Xdmf DSM client mode
    //
    vtkSmartPointer<vtkDSMManager> DSMManager = NULL;
    DSMManager = vtkSmartPointer<vtkDSMManager>::New();
    DSMManager->SetDsmIsServer(1);
    DSMManager->SetController(controller);
    DSMManager->SetDsmCommType(H5FD_DSM_COMM_SOCKET);
    DSMManager->SetServerPort(default_port_number);
    DSMManager->SetServerHostName(server_name.c_str());
    DSMManager->CreateDSM();
    // This will write out .dsm_config config file
    DSMManager->PublishDSM();

//    // Create a thread object to poll our DSM for new contents
//#ifdef HAVE_PTHREADS
//    pthread_create(&this->ServiceThread, NULL, &H5FDdsmBufferServiceThread, (void *) this->DSMBuffer);
//#elif HAVE_BOOST_THREADS
//    DSMListenThread MyDSMListenThread(DSMManager);
//    ServiceThread = new boost::thread(MyDSMListenThread);
//#endif

    H5FDdsmInt64   Counter = 0;
    H5FDdsmInt64   UpdatesCounter = 0;
    vtkSmartPointer<vtkXdmfReader4> XdmfReader = vtkSmartPointer<vtkXdmfReader4>::New();
    XdmfReader->SetController(controller);
    XdmfReader->SetDSMManager(DSMManager);
    XdmfReader->SetFileName("stdin");

    bool           good = true;  
    while(good) {
      UpdatesCounter ++;
      if (DSMManager->GetDsmUpdateReady()) {
        std::cout << UpdatesCounter << " : " << ++Counter << std::endl;
        // H5Dump
        DSMManager->H5DumpLight();
        // Xdmf
        XdmfReader->Update();
        vtkDataObject *data = XdmfReader->GetOutputDataObject(0);
        data->PrintSelf(std::cout, vtkIndent(0));
        // Clean up for next step
        controller->Barrier();
        DSMManager->ClearDsmUpdateReady();
        DSMManager->RequestRemoteChannel();
        if (Counter>=10000) {
          good = false;
        }
      }

      //if (UpdatesCounter%10000 == 0) {
      //  std::cout << UpdatesCounter << " : " << Counter << std::endl;
      //  std::cout.flush();
      //}
  //    ::Sleep(100);
    }

    std::cout.flush();
    std::cout << "\n\n\nClosing down DSM server \n\n\n"<< std::endl;
    std::cout.flush();

//    ServiceThread->join();
    //
    controller->Barrier();
    DSMManager->UnpublishDSM();
   } // end scope block
}

//----------------------------------------------------------------------------
int main (int argc, char* argv[])
{
  // This is here to avoid false leak messages from vtkDebugLeaks when
  // using mpich. It appears that the root process which spawns all the
  // main processes waits in MPI_Init() and calls exit() when
  // the others are done, causing apparent memory leaks for any objects
  // created before MPI_Init().
  int provided, rank, size;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
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

  // Note that this will create a vtkMPIController if MPI
  // is configured, vtkThreadedController otherwise.
  vtkMPIController* controller = vtkMPIController::New();

  controller->Initialize(&argc, &argv, 1);

  vtkParallelFactory* pf = vtkParallelFactory::New();
  vtkObjectFactory::RegisterFactory(pf);
  pf->Delete();
 
  // Added for regression test.
  // ----------------------------------------------
  int retVal = 1;
  ParallelArgs_tmp args;
  args.retVal = &retVal;
  args.argc = argc;
  args.argv = argv;
  // ----------------------------------------------

  controller->SetSingleMethod(MyMain, &args);
  controller->SingleMethodExecute();

  controller->Barrier();
  controller->Finalize();
  controller->Delete();

  return !retVal;
}
