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

// For resampling and redistributing
#include "vtkSplitBlocksFilter.h"
#include "vtkRedistributeBlocksFilter.h"
#include "vtkSubdivideUnstructuredGrid.h"

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
#include "vtkXdmfReader4.h"

#ifdef HAS_CFX
  #include "vtkCFXReader.h"
#endif

#ifdef HAS_FLUENT
  #include "vtkFluentReaderCSCS.h"
#endif

// CSCS
//#include "vtkStreamOutputWindow.h"

// Xdmf/DSM features 
#include "H5FDdsm.h"
#include "vtkDSMManager.h"
#include "H5FDdsmDump.h"

// Sys
#include <sstream>
#include <mpi.h>

#define PARALLEL_PIECES

typedef std::vector<double>::size_type itype;

#ifdef MACHINE_AGNO
  #define server "agno.staff.cscs.ch"
  #define client "dino.staff.cscs.ch"
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
vtkSmartPointer<vtkAlgorithm> SetupDataSetReader(const char *filename) {
  vtkSmartPointer<vtkXMLUnstructuredGridReader> reader = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
  reader->SetFileName(filename);
  return reader;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkAlgorithm> SetupMultiBlockReader(const char *filename) {
  vtkSmartPointer<vtkXMLMultiBlockDataReader> reader = vtkSmartPointer<vtkXMLMultiBlockDataReader>::New();
  reader->SetFileName(filename);
  return reader;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkAlgorithm> SetupCollectionReader(const char *filename) {
  vtkSmartPointer<vtkPVDReader> reader = vtkSmartPointer<vtkPVDReader>::New();
  reader->SetFileName(filename);
  return reader;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkAlgorithm> SetupPvtiReader(const char *filename) {
  vtkSmartPointer<vtkXMLPImageDataReader> reader = vtkSmartPointer<vtkXMLPImageDataReader>::New();
  reader->SetFileName(filename);
  return reader;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkAlgorithm> SetupXdmfReader(const char *filename) {
  vtkSmartPointer<vtkXdmfReader> reader = vtkSmartPointer<vtkXdmfReader>::New();
  reader->SetFileName(filename);
  return reader;
}
//----------------------------------------------------------------------------
vtkSmartPointer<vtkAlgorithm> SetupXdmfReader3(const char *filename) {
  vtkSmartPointer<vtkXdmfReader4> reader = vtkSmartPointer<vtkXdmfReader4>::New();
  reader->SetFileName(filename);
  return reader;
}

//----------------------------------------------------------------------------
#ifdef HAS_CFX
vtkSmartPointer<vtkAlgorithm> SetupCFXReader(const char *filename) {
  vtkSmartPointer<vtkCFXReader> reader = vtkSmartPointer<vtkCFXReader>::New();
  reader->SetFileName(filename);
  reader->SetLoadBoundaries(0);
  reader->SetRotateRotatingZoneVectors(1);
  reader->SetLoadVolumes(1);
  return reader;
}
#endif
//----------------------------------------------------------------------------
#ifdef HAS_FLUENT
vtkSmartPointer<vtkAlgorithm> SetupFluentReader(const char *filename, bool flip) {
  vtkSmartPointer<vtkFluentReaderCSCS> reader = vtkSmartPointer<vtkFluentReaderCSCS>::New();
  reader->SetDataByteOrderToLittleEndian();
  reader->SetFileName(filename);
  reader->SetTimeStep(0);
  reader->SetFlip180Degrees(flip);
  return reader;
}
#endif
//----------------------------------------------------------------------------

// Just pick a tag which is available
static const int RMI_TAG=300; 

struct ParallelArgs_tmp
{
  int* retVal;
  int    argc;
  char** argv;
};
//----------------------------------------------------------------------------
struct ParallelRMIArgs_tmp
{
  vtkMultiProcessController* Controller;
};
//----------------------------------------------------------------------------
// Call back we can use for future control of processes
void SetStuffRMI(void *localArg, void* vtkNotUsed(remoteArg), 
                    int vtkNotUsed(remoteArgLen), int vtkNotUsed(id))
{ 
  // ParallelRMIArgs_tmp* args = (ParallelRMIArgs_tmp*)localArg;
  // vtkMultiProcessController* contrl = args->Controller;
}
//----------------------------------------------------------------------------
// This will be called by all processes
void MyMain( vtkMultiProcessController *controller, void *arg )
{
  // Obtain the id of the running process and the total
  // number of processes
  vtkTypeInt64 myId = controller->GetLocalProcessId();
  vtkTypeInt64 numProcs = controller->GetNumberOfProcesses();

  if (myId==0) {
    std::cout << usage.c_str() << std::endl;
    std::cout << "Process number " << myId << " of " << numProcs << std::endl;
  }
  else {
    std::cout << "Process number " << myId << " of " << numProcs << std::endl;
  }
  controller->Barrier();

  //
  // begin scope block so auto-deletion of memory can be checked
  //
  { 
    //
    // Force the creation of our output window object
    //
//    vtkSmartPointer<vtkStreamOutputWindow> outwin = vtkSmartPointer<vtkStreamOutputWindow>::New();
//    vtkOutputWindow::SetInstance(outwin);
//    outwin->SetOutputStream(&std::cout);

    //
    // Use utilities in vtkTesting for getting command line params
    //
    ParallelArgs_tmp* args = reinterpret_cast<ParallelArgs_tmp*>(arg);
    vtkSmartPointer<vtkTesting> test = vtkSmartPointer<vtkTesting>::New();
    for (int c=1; c<args->argc; c++ ) {
      test->AddArgument(args->argv[c]);
    }

    //
    // Is DSM Client mode set
    //
    bool DSMclient = false;
    if (test->IsFlagSpecified("-Client")) {
      std::cout << "DSMclient (-Client flag) set" << std::endl;
      DSMclient = true;
    }

    //
    // Is DSM Server mode set
    //
    bool DSMserver = false;
    if (test->IsFlagSpecified("-Server")) {
      std::cout << "DSMserver (-Server flag) set" << std::endl;
      DSMserver = true;
    }

    //
    // Is DSM Server mode set
    //
    bool DSMstandalone = false;
    if (test->IsFlagSpecified("-DSM")) {
      std::cout << "DSMstandalone (-DSM flag) set" << std::endl;
      DSMstandalone = true;
    }

    //
    // Extract filename from test parameters
    //
    char *filename = vtkTestUtilities::GetArgOrEnvOrDefault(
      "-F", args->argc, args->argv, "DUMMY_ENV_VAR", 
      // Default file name
      "unused.abc");
    char* inputname = NULL;
    // Client does not load anything from file
    if (!DSMserver) {
      std::cout << "ConvertToXdmf " << std::endl;
      std::cout << "Got Filename " << filename << std::endl;
      inputname = vtkTestUtilities::ExpandDataFileName(args->argc, args->argv, filename);
      std::cout << "Got Fullname " << inputname << std::endl << std::endl;
      if (!vtksys::SystemTools::FileExists(inputname)) {
        std::cout << "Invalid filename " << inputname << std::endl;
        return;
      }
    }

  #ifndef WIN32
    char *out_dir = vtkTestUtilities::GetArgOrEnvOrDefault(
      "-O", args->argc, args->argv, "DUMMY_ENV_VAR", 
      // Default file name
      "/projects/biddisco/MHD");
  #else
    char *out_dir = vtkTestUtilities::GetArgOrEnvOrDefault(
      "-O", args->argc, args->argv, "DUMMY_ENV_VAR", 
      // Default file name
      "D:\\Temp\\");
  #endif
    vtkstd::string file_prefix = vtksys::SystemTools::GetFilenameWithoutExtension(filename);
    vtkstd::string xdmf_name   = vtkstd::string(out_dir) + "/" + file_prefix;
    vtkstd::string extension   = vtksys::SystemTools::GetFilenameExtension(filename);
    vtkstd::string domainname  = file_prefix;

    //
    // Is static flag present
    //
    bool Static = false;
    if (test->IsFlagSpecified("-s")) {
      std::cout << "Static (-s flag) set" << std::endl;
      Static = true;
    }

    //
    // Is rotate flag present
    //
    bool rotate = false;
    if (test->IsFlagSpecified("-r")) {
      std::cout << "Rotate (-r flag) set" << std::endl;
      rotate = true;
    }

    //
    // Is Subdivide flag present
    //
    bool subdivide = false;
    if (test->IsFlagSpecified("-v")) {
      std::cout << "Subdivide (-v flag) set" << std::endl;
      subdivide = true;
    }

    //
    // is Flip flag specified
    //
    bool flip = false;
    if (test->IsFlagSpecified("-f")) {
      std::cout << "Flipping 180 degrees " << std::endl;
      flip = true;
    }

    //
    // Is append flag present
    //
    bool append = false;
    if (test->IsFlagSpecified("-a")) {
      std::cout << "Append (-a flag) set" << std::endl;
      rotate = true;
    }

    if (!append && vtksys::SystemTools::FileExists((xdmf_name+".xmf").c_str())) {
      vtksys::SystemTools::RemoveFile((xdmf_name+".xmf").c_str());
    }

    //
    // Interpolation requested?
    //
    double interpolation = 0;
    char *number = vtkTestUtilities::GetArgOrEnvOrDefault(
      "-i", args->argc, args->argv, "DUMMY_ENV_VAR", "");
    if (std::string(number)!=std::string("")) {
      vtkstd::stringstream temp;
      temp << number;
      temp >> interpolation;
      std::cout << "Interpolating with timestep : " << interpolation << std::endl;
    }
    std::cout << std::endl << std::endl;

    //
    //
    //
    if (!append) {
      vtksys::SystemTools::RemoveFile(vtkstd::string(xdmf_name+".xmf").c_str());
      vtksys::SystemTools::RemoveFile(vtkstd::string(xdmf_name+".h5").c_str());
    }

    //
    // Start the pipeline with a reader
    //
    vtkSmartPointer<vtkAlgorithm> algorithm, reader;
    if (extension==".vtu") {
      reader = SetupDataSetReader(inputname);
    }
    else if (extension==".pvd") {
      reader = SetupCollectionReader(inputname);
    }
    else if (extension==".pvti") {
      reader = SetupPvtiReader(inputname);
    }    
    else if (extension==".xmf" || extension==".xdmf") {
      reader = SetupXdmfReader(inputname);
    }
#ifdef HAS_CFX
    else if (extension==".res") {
      reader = SetupCFXReader(inputname);
    }
#endif
#ifdef HAS_FLUENT
    else if (extension==".cas" || extension==".cas2") {
      reader = SetupFluentReader(inputname, flip);
    }
#endif
    algorithm = reader;

    vtkSmartPointer<vtkCompositeDataPipeline> defaultexecutive = 
      vtkSmartPointer<vtkCompositeDataPipeline>::New();
    vtkAlgorithm::SetDefaultExecutivePrototype(defaultexecutive);      

    //
    // Subdivide data into smaller pieces?
    //
    vtkSmartPointer<vtkSplitBlocksFilter>         blocks;
    vtkSmartPointer<vtkRedistributeBlocksFilter>  redist;
    vtkSmartPointer<vtkSubdivideUnstructuredGrid> divide;
    if (subdivide) {
      blocks = vtkSmartPointer<vtkSplitBlocksFilter>::New();
      blocks->SetInputConnection(algorithm->GetOutputPort());
      blocks->SetMaximumCellsPerBlock(65536);
      //
      redist = vtkSmartPointer<vtkRedistributeBlocksFilter>::New();
      redist->SetInputConnection(blocks->GetOutputPort());
      //
      divide = vtkSmartPointer<vtkSubdivideUnstructuredGrid>::New();
      divide->SetInputConnection(redist->GetOutputPort());
      algorithm = divide;
    }

    //
    // Interpolate data between time steps?
    //
    vtkSmartPointer<vtkTemporalDataSetCache> cache = vtkSmartPointer<vtkTemporalDataSetCache>::New();
    vtkSmartPointer<vtkTemporalInterpolator> inter = vtkSmartPointer<vtkTemporalInterpolator>::New();
    if (interpolation>0) {
      cache->SetInputConnection(algorithm->GetOutputPort());
      cache->SetCacheSize(2);
      inter->SetInputConnection(cache->GetOutputPort());
      inter->SetDiscreteTimeStepInterval(interpolation);
      //
      algorithm = inter;
    }

    //
    // Set Update piece information so pipeline can split correctly
    // actually, it's invalid to do it here, but we're testing
    //
    vtkSmartPointer<vtkInformation> execInfo;
    if (!DSMserver) {
      execInfo = algorithm->GetExecutive()->GetOutputInformation(0);
      execInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES(), numProcs);
      execInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER(), myId);

      //
      // update information to set/get TIME_XXX keys and Extents
      //
      algorithm->UpdateInformation();

      //
      // Split into pieces for parallel loading
      //
      // int MaxPieces = execInfo->Get(vtkStreamingDemandDrivenPipeline::MAXIMUM_NUMBER_OF_PIECES());
      int WholeExtent[6], UpdateExtent[6];
      execInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), WholeExtent);
      vtkSmartPointer<vtkExtentTranslator> extTran = vtkSmartPointer<vtkExtentTranslator>::New();
        extTran->SetSplitModeToBlock();
        extTran->SetNumberOfPieces(numProcs);
        extTran->SetPiece(myId);
        extTran->SetWholeExtent(WholeExtent);
        extTran->PieceToExtent();
        extTran->GetExtent(UpdateExtent);
      execInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), UpdateExtent, 6);
    }

    //
    // Get time values
    //    
    vtkstd::vector<double> TimeSteps;
    if (execInfo && execInfo->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS())) 
    {
      int NumberOfTimeSteps = execInfo->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
      TimeSteps.assign(NumberOfTimeSteps, 0.0);
      execInfo->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS(), &TimeSteps[0]);
    }
    else {
      TimeSteps.assign(1, 0.0);
    }


    //
    // if we are running a parallel job, sync here
    //
    controller->Barrier();

    //-------------------------------------------------------------
    // Limit time steps for testing to 3 so we don't wait all day
    //-------------------------------------------------------------
    itype t0=0, t1=TimeSteps.size();
//    t1 = TimeSteps.size()>3 ? 3 : TimeSteps.size();

    //
    // Create the Xdmf Writer
    //
    vtkSmartPointer<vtkXdmfWriter4> writer = vtkSmartPointer<vtkXdmfWriter4>::New();

    //
    // If set, setup Xdmf DSM client mode
    //
    vtkSmartPointer<vtkDSMManager> DSMManager = NULL;
    if (DSMserver || DSMclient || DSMstandalone) {
      DSMManager = vtkSmartPointer<vtkDSMManager>::New();
      if (DSMserver || DSMstandalone) {
        DSMManager->SetDsmIsServer(1);
      }
      else {
        DSMManager->SetDsmIsServer(0);
      }
      DSMManager->SetController(controller);
      DSMManager->SetDsmCommType(XDMF_DSM_COMM_SOCKET);
      DSMManager->SetServerPort(default_port_number);
      DSMManager->SetServerHostName(server_name.c_str());
      DSMManager->CreateDSM();
      if (DSMserver) {
        DSMManager->PublishDSM();
      }
      if (DSMclient) {
        DSMManager->ConnectDSM();
      }     
      if (DSMclient || DSMstandalone) {
        writer->SetDSMManager(DSMManager);
      }
    }

    //
    // Write out time steps to File or DSM.
    // Note : DSM Server receives data but writes nothing
    //
    if (!DSMserver) {
      for (itype t=t0; t<t1; ++t) {
        //
        // Update the time we want
        //
        itype TimeStep = t; // was missingsteps[t]
        double current_time = TimeSteps[TimeStep];
        //
        vtkstd::cout << "Beginning TimeStep " << setw(5) << setfill('0') << t 
          << " for time " << setw(8) << setfill('0') << current_time << vtkstd::endl;
        //
        execInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEPS(), &current_time, 1);
#ifdef PARALLEL_PIECES
        execInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES(), numProcs);
        execInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER(), myId);
#endif
        algorithm->Update();
        //
        // get the output temporal/multiblock - depends if we are interpolating or not
        //
        vtkSmartPointer<vtkTemporalDataSet>     temporal = vtkTemporalDataSet::SafeDownCast(algorithm->GetOutputDataObject(0));
        vtkSmartPointer<vtkMultiBlockDataSet> multiblock = vtkMultiBlockDataSet::SafeDownCast(algorithm->GetOutputDataObject(0));
        vtkSmartPointer<vtkDataSet>              dataset = vtkDataSet::SafeDownCast(algorithm->GetOutputDataObject(0));
        if (temporal) {
          multiblock = vtkMultiBlockDataSet::SafeDownCast(temporal->GetTimeStep(0));
        }

        //
        // int d=0;
        if (multiblock || dataset) {

          if (multiblock) writer->SetInput(multiblock);
          if (dataset)    writer->SetInput(dataset);
          //
          writer->SetController(controller);
          writer->SetTopologyConstant(Static);
          writer->SetGeometryConstant(Static);
          writer->SetDomainName(domainname.c_str());
          writer->SetFileName(xdmf_name.c_str());
          writer->SetTimeStep((int)TimeStep);
          writer->Modified();
          writer->Update();
        }

        // At the end of each time step, close the file. 
        // When using DSM, this triggers an update at the server end.
        writer->CloseHDFFile();
      }
    }

    //
    // We must disconnect our client 
    //
    if (DSMclient) {
      writer = NULL; // forces close of everything
      controller->Barrier();
      DSMManager->DisconnectDSM();
    }

    //
    // DSM Client needs to disply contents of what it has received
    //
    if (DSMserver || DSMstandalone) {
      if (myId==0) {
        std::cout << "Please press a key to dump DSM contents" << std::endl;
        char ch;
        std::cin >> ch;
      }
      //
      controller->Barrier();
      vtkSmartPointer<vtkXdmfReader4> reader = vtkXdmfReader4::SafeDownCast(SetupXdmfReader3(""));
      reader->SetController(controller);
      reader->SetDSMManager(DSMManager);
      reader->Update();
      DSMManager->H5DumpLight();

    }
    //
    // After all time steps have been written.
    //
    delete []inputname;
    delete []filename;
    //
    controller->Barrier();
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
