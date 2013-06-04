#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>
#include <mpi.h>
#include "H5FDdsm.h"
#include "H5FDdsmManager.h"
#include "H5FDdsmSteering.h"
#include "hdf5.h"
#include "hdf5_hl.h"
#ifdef _WIN32
 #define NOMINMAX 
 #include <windows.h>
 #define sleep(a) Sleep(a)
#endif

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
// Example which generates a grid and writes it to DSM or Disk as hdf5.
//
// Before running you will need to ensure that your path is set up
// In all examples to follow, please edit the paths used to refect your own build.
// The path should include HDF5,H5FDdsm,paraview,plugins,qt directories
// For my current setup I use 
// set PATH=D:\build\plugins-3.98>set PATH=C:\Program Files\HDF5-vs11\bin;C:\Program Files\h5fddsm\bin;C:\Qt\4.8.3\bin;D:\build\plugins-3.98\bin\Debug;D:\build\paraview-3.98\bin\Debug
// then execute the test "IcarusTestDriver"
// D:\build\plugins-3.98\bin\Debug\IcarusTestDriver.exe
//
// IcarusTestDriver with no arguments or arg = 0 to attempt a dsm connection and live demo
// IcarusTestDriver with arg = non-zero integer to create a simple hdf5 file.
//
// ==============
// Usage method 1
// ==============
//
// If a connection with dsm and paraview is not possible, generate an
// hdf5 file and then run the xdmfgenerator on the hdf5 file to generate an
// xdmf file which you can then load into paraview to test the xdmf capabilities
// To generate Xdmf, you would use a command such as 
// D:\build\plugins-3.98\bin\Debug\IcarusTestDriver.exe 1
// then 
// D:\build\plugins-3.98>D:\build\plugins-3.98\bin\Debug\XdmfGenerate.exe D:\Code\plugins\Icarus\DSMManager\example\IcarusTestDriver.xmf D:\build\plugins-3.98\TriangleGrid.h5 0
// and pipe the result to a file which will look similar to the following 
//
// <?xml version="1.0" ?>
// <!DOCTYPE Xdmf SYSTEM "Xdmf.dtd" []>
// <Xdmf xmlns:xi="http://www.w3.org/2003/XInclude" Version="2.1">
//   <Domain>
//     <Grid Name="HeppelGrid" GridType="Uniform">
//       <Topology TopologyType="Triangle" Dimensions="44402">
//        <DataItem Dimensions="44402 3" NumberType="Int" Precision="4" Format="HDF">File:D:/build/plugins-3.98/TriangleGrid.h5:/topology</DataItem>
//       </Topology>
//       <Geometry GeometryType="XYZ">
//        <DataItem Dimensions="22500 3" NumberType="Float" Precision="8" Format="HDF">File:D:/build/plugins-3.98/TriangleGrid.h5:/Points</DataItem>
//       </Geometry>
//      <Attribute Name="PointHeight" AttributeType="Scalar" Center="Node">
//        <DataItem Dimensions="22500" NumberType="Float" Precision="4" Format="HDF">File:D:/build/plugins-3.98/TriangleGrid.h5:/Height</DataItem>
//      </Attribute>
//      <Attribute Name="CellHeight" AttributeType="Scalar" Center="Cell">
//        <DataItem Dimensions="44402" NumberType="Float" Precision="4" Format="HDF">File:D:/build/plugins-3.98/TriangleGrid.h5:/HeightCell</DataItem>
//      </Attribute>
//     </Grid>
//  </Domain>
// </Xdmf>
// This can be loaded into paraview and the mesh should be visible.
//
// ==============
// Usage method 2
// ==============
// First, run paraview with the icarus plugin loaded.
// Select the "Basic Controls" tab of the dsm panel.
// Select "XDMF Template format" in the Description file format combo.
// Make sure the full path to the xdmf template D:\Code\plugins\Icarus\DSMManager\example\IcarusTestDriver.xmf
// is set in the "Description file path"
// make sure "Buffer Size/Node" is >0, use 32 or 64MB for experimenting.
// Select the intercommunicator as MPI or sockets.
// Use static comunicator should be Unset.
// "Force Xdmf generation" must be enabled.
// Click enable_dsm to activate the DSM server
// Run the test generator without arguments and a connection should be established
// with a dataset becoming visible inside paraview.
// Select the "Steering Controls" tab and notice that "Iterate step by step" is set.
// Unset the step by step and click apply. The surface should animate. 
// You can rotate it and add filters to it, just as you would any dataset.
// Click Pause. and the animation should halt.
// Notice that the Command Buttons (Pause/Shutdown/etc) take immediate effect
// whereas the parameter buttons wait for "apply" to be clicked before occurring.
// The option "Force XDMF generation" this tells the plugin to regenerate the xdmf file every time new data is received.
// When a mesh is fixed and the number of scalars/fields does not change, a single xdmf file
// can be used for every time step. But if the mesh changes dimension or other attributes are
// varying, then an xdmf file at one time step will be invalid at another. 
// For this reason we cannot vary yhe mesh size (X/Y resolution) if the regeneration is not
// enabled (a crash will occur if we try it). Experiment with changing the X/Y resolution
// and see how the display updates.
// 
//
//
//
//
//
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
double gaussian2D(double x, double y, double sigma) { 
  return std::exp(-(x*x + y*y) / (2.0 * sigma * sigma));// / (2.0 * M_PI * sigma * sigma);
}
//----------------------------------------------------------------------------
void createMesh(int XSize, int YSize, 
  std::vector<double> &vertices, 
  std::vector<unsigned int> &faces, 
  std::vector<float> &pointscalar, 
  std::vector<float> &cellscalar, 
  double timevalue) 
{
  //
  // Create vertices and vertex point data
  //
  const size_t nVertices = XSize*YSize;
  const size_t vbufCount = 3*nVertices;

  vertices.resize(vbufCount);
  pointscalar.resize(nVertices);

  size_t vertex = 0, vBufCounter = 0;
  double xscale = 10.0/XSize, xmid = XSize/2.0; 
  double yscale = 10.0/YSize, ymid = YSize/2.0; 
  for (int z = 0; z < YSize; z++) {
    for (int x = 0; x < XSize; x++) {
      // Scalar Field
      double height = std::cos(timevalue*M_PI/3 + 2.0*M_PI*5*static_cast<double>(x)/XSize)
                     *std::cos(timevalue*M_PI/4 + 2.0*M_PI*7*static_cast<double>(z)/YSize)
                     *(0.05 + 0.25*gaussian2D((z - YSize/2.0), (x-XSize/2.0), std::min(XSize,YSize)/4.0));
      pointscalar[vertex++] = static_cast<float>(height);

      // Position
      vertices[vBufCounter]   = (x-xmid)*xscale;
      vertices[vBufCounter+1] = height*3.0;
      vertices[vBufCounter+2] = (z-ymid)*yscale;

      vBufCounter += 3;
    }
  }

  //
  // Create faces (triangles) and cell data
  // (2 tris per grid cell, hence list is 6* not 3* bigger)
  //
  const size_t nCells    = 2*(XSize - 1)*(YSize - 1);
  const size_t ibufCount = 3*nCells;
  faces.resize(ibufCount);
  cellscalar.resize(nCells);
  //
  size_t iBufCounter = 0, index = 0;
  for (int z=0; z < YSize-1; z++) {
    for (int x=0; x < XSize-1; x++) {

      double height = std::cos(timevalue*M_PI/3 + 2.0*M_PI*5*static_cast<double>(x)/XSize)
                     *std::cos(timevalue*M_PI/4 + 2.0*M_PI*7*static_cast<double>(z)/YSize)
                     *(0.05 + 0.25*gaussian2D((z - YSize/2.0), (x-XSize/2.0), std::min(XSize,YSize)/4.0));
      // 2 triangles per iteration
      cellscalar[index++] = static_cast<float>(height);
      cellscalar[index++] = static_cast<float>(height);

      // triangle 1
      faces[iBufCounter]   = ((z+0)*XSize) +  x;
      faces[iBufCounter+1] = ((z+1)*XSize) +  x;
      faces[iBufCounter+2] = ((z+1)*XSize) + (x+1);

      // triangle 2
      faces[iBufCounter+3] = ((z+1)*XSize) + (x+1);
      faces[iBufCounter+4] = ((z+0)*XSize) +  x;
      faces[iBufCounter+5] = ((z+0)*XSize) + (x+1);

      iBufCounter += 6;
    }
  }
}
//----------------------------------------------------------------------------
int main(int argc, char *argv[])  
{ 
  int usedsm = 1, staticInterComm=0;
  int size=1, rank=0;
  int nc = 1;
  hid_t file_id, fapl_id;
  herr_t err;

  H5FDdsmManager *dsmManager = NULL;
  MPI_Comm comm = MPI_COMM_WORLD;

  if (argc>1) {
    usedsm = !atoi(argv[1]);
  }
  if (usedsm) {
    nc = 50;
    // start MPI. We are using mpich2
    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cout << "size = " << size << "  rank= " << rank << std::endl;

    //
    // Create a DSM manager and read server generated config
    //
    dsmManager = new H5FDdsmManager();
    dsmManager->ReadConfigFile();
    // if using a static commmunicator, set it up. Untested here : For advanced users!
    if (dsmManager->GetUseStaticInterComm()) {
      H5FDdsmInt32 color = 2; // 1 for server, 2 for client
      MPI_Comm_split(MPI_COMM_WORLD, color, rank, &comm);
      MPI_Comm_rank(comm, &rank);
      MPI_Comm_size(comm, &size);
      std::cout << "size = " << size << "  rank= " << rank << std::endl;
      staticInterComm = H5FD_DSM_TRUE;
    }
    // trigger connection using all our settings
    dsmManager->SetMpiComm(comm);
    dsmManager->SetIsServer(H5FD_DSM_FALSE);
    dsmManager->Create();
    // set the global manager to ours (only ever use one manager)
    H5FD_dsm_set_manager(dsmManager);
    H5FD_dsm_steering_init(comm);
  }

  //
  // some arrays to use for an unstructured grid (triangle grid in our example)
  //
  std::vector<double>       vertices;
  std::vector<float>        pointscalars;
  std::vector<float>        cellscalars;
  std::vector<unsigned int> faces;

  int stop        = 0;
  int pause       = 0;
  int stepmode    = 1;
  int delay       = 250;
  int XResolution = 150;
  int YResolution = 150;

  //
  // this loop sends a slightly different dataset each iteration.
  //
  double timevalue = 0.0;
  while (!stop) {

    //
    // Write data out to DSM or disk
    //
    fapl_id = H5Pcreate(H5P_FILE_ACCESS); 		
    // std::cout << "H5Pcreate done " << std::endl;

    if (usedsm) {
      err = H5Pset_fapl_dsm(fapl_id, comm, NULL, 0); 	
      // std::cout << "H5Pset_fapl_dsm done : " << std::endl;
      //
      // To ensure good synchronization when reading steering values from the DSM
      //  1) lock the dsm 
      //  2) read any data sent by the gui/other application
      //  2) write new data to the dsm
      //  3) unlock the dsm
      // We always read data before writing to prevent the write from wiping anything
      // that we might have wanted to read.

      // std::cout << "DSM locked" << std::endl;
      H5FD_dsm_lock();

      //
      // cache some handles to make multiple accesses as efficient as possible
      //
      H5FD_dsm_steering_begin_query();
        //
        // Read any values that the user might have changed
        // 
        if (dsmManager->GetSteeringValues("Pause", 1, &pause)) {
          std::cout << "Received a Pause command" << std::endl; 
        };
        if (dsmManager->GetSteeringValues("Shutdown", 1, &stop)) {
          std::cout << "Received a Shutdown command" << std::endl; 
        };
        if (dsmManager->GetSteeringValues("StepMode", 1, &stepmode)) {
          std::cout << "Received a StepMode change" << std::endl; 
        };
        if (dsmManager->GetSteeringValues("XResolution", 1, &XResolution)) {
          std::cout << "Received an X resolution change " << XResolution << std::endl; 
        };
        if (dsmManager->GetSteeringValues("YResolution", 1, &YResolution)) {
          std::cout << "Received a Y resolution change " << YResolution << std::endl; 
        };
        if (dsmManager->GetSteeringValues("Delay", 1, &delay)) {
          std::cout << "Received a Delay change " << delay << std::endl; 
        };
      H5FD_dsm_steering_end_query();

      file_id = H5Fcreate("dsm", H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id); 
    }
    else {
      file_id = H5Fcreate("TriangleGrid.h5", H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id);
    }

    // std::cout << "H5Fcreate done" << std::endl;
    H5Pclose(fapl_id);

    //
    // generate a mesh using time=t
    //
    createMesh(XResolution, YResolution, vertices, faces, pointscalars, cellscalars, timevalue);
    // we need to know how many points/faces were generated
    int nPts   = static_cast<int>(vertices.size()/3);
    int nfaces = static_cast<int>(faces.size()/3);

    //
    // write out arrays to define our UNSTRUCTURED Grid
    //
    // Topology
    {
      // Number of Triangles <DataItem>/topology</DataItem>, 
      // for mixed geometry types a more complex dataset would be required
      hsize_t dims[2] = {nfaces, 3}; 
      herr_t status = H5LTmake_dataset(file_id, "/topology",2, dims, H5T_NATIVE_UINT, &faces[0]);
    }

    // Geometry
    {
      hsize_t dims[2] = {nPts, 3};
      herr_t status = H5LTmake_dataset(file_id, "/Points", 2, dims, H5T_NATIVE_DOUBLE, &vertices[0]);  
    }

    // Attributes (per point/node)
    {
      hsize_t dims[1] = {nPts}; 
      herr_t status = H5LTmake_dataset(file_id, "/Height", 1, dims, H5T_NATIVE_FLOAT, &pointscalars[0]);  
    }

    // Attributes (per cell)
    {
      hsize_t dims[1] = {nfaces}; 
      herr_t status = H5LTmake_dataset(file_id, "/HeightCell", 1, dims, H5T_NATIVE_FLOAT, &cellscalars[0]);  
    }

    //
    // manually unlock (release) the DSM and send a NEW_DATA message
    //
    if (usedsm) {
      H5Fclose(file_id);
      // std::cout << "DSM unlocked" << std::endl;

      H5FD_dsm_steering_begin_query();
      // Line plot data
      {
        double h = cellscalars[0];
        H5FD_dsm_steering_scalar_set("TimeValue",  H5T_NATIVE_DOUBLE, &timevalue);
        H5FD_dsm_steering_scalar_set("ExampleHeight", H5T_NATIVE_DOUBLE, &h);
      }
      H5FD_dsm_steering_end_query();

      // release our hold on the dsm and notify server we have sent something
      H5FD_dsm_unlock(H5FD_DSM_NOTIFY_DATA);

      // if we are due to wait, go ahead and wait
      if (!stop && (pause || stepmode)) {
        // pause the simulation
        H5FD_dsm_steering_wait();
        pause = 0;
      }
      else {
        sleep(delay);
      }
    }
    // writing to file, close it
    else {
      H5Fclose(file_id);
      std::cout << "File written " << std::endl;
      stop = 1;
    }

    timevalue += 0.05;
  } 
  H5close();


  if (usedsm) {
    dsmManager->Disconnect();
    //
    dsmManager->Destroy();
    if (staticInterComm) {
      MPI_Barrier(comm);
      MPI_Comm_free(&comm);
    }
    MPI_Finalize();
  }
  return 0;
}
//----------------------------------------------------------------------------
