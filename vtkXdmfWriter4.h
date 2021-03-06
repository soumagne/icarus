/*=========================================================================

  Project                 : Icarus
  Module                  : vtkXdmfWriter4.h

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
// .NAME vtkXdmfWriter4 - Write Xdmf + HDF5 files from vtk input
// .SECTION Description
// This writer takes vtk input data and writes it out to Xdmf xml + hdf5
// files. It uses xdmf structures and DOM objects to manipulate files
// and is therefore cleaner than the original vtkXmdfWriter class
// which manipulated xml strings directly.
// Currently this writer has only limited functionality, but should
// gradually improve to eventually surpass the earlier writer.

#ifndef __vtkXdmfWriter4_h
#define __vtkXdmfWriter4_h

#include "vtkSmartPointer.h" // For vtkSmartPointer
#include "vtkXdmfWriter.h"
#include <vtkstd/string>

class vtkMultiProcessController;
class vtkPointSet;
class vtkDataArray;
class vtkPointData;
class vtkUnstructuredGrid;
class vtkDataSet;
class vtkMultiBlockDataSet;
class vtkDsmManager;
//BTX
class H5MBCallback;
//ETX

class VTK_EXPORT vtkXdmfWriter4 : public vtkXdmfWriter
{
public:
  static vtkXdmfWriter4 *New();
  vtkTypeRevisionMacro(vtkXdmfWriter4,vtkXdmfWriter);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // For parallel write, set Collective or Independent
  vtkSetMacro(CollectiveIO,int);
  vtkGetMacro(CollectiveIO,int);
  void SetWriteModeToCollective();
  void SetWriteModeToIndependent();

//BTX
  enum probetypes {
    VTK_XDMF_APPEND_OVERWRITE  = 0,
    VTK_XDMF_APPEND_MULTIBLOCK = 1,
    VTK_XDMF_APPEND_TEMPORAL   = 2,
  };
//ETX

//BTX
  enum buildmodes {
    VTK_XDMF_BUILD_ALL = 0,
    VTK_XDMF_BUILD_LIGHT = 1,
    VTK_XDMF_BUILD_HEAVY = 2,
  };
//ETX

  // Description:
  // Overwrite or append to existing xdmf files.
  // When looping over time, Append mode temporal will make
  // use of the TimeStep variable to generate unique names for
  // files and datagroups/datasets
  vtkSetMacro(AppendMode,int);
  vtkGetMacro(AppendMode,int);
  void SetAppendModeToOverwrite();
  void SetAppendModeToAppendMultiBlock();
  void SetAppendModeToAppendTemporal();

  // Description:
  // Set/Get the controller used for coordinating parallel writing
  // (set to the global controller by default)
  // If not using the default, this must be called before any
  // other methods.
  virtual void SetController(vtkMultiProcessController* controller);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);

  // Description:
  // Set/Get DsmBuffer Manager and enable DSM data writing instead of using
  // disk. The DSM manager is assumed to be created and initialized before
  // being passed into this routine
  virtual void SetDsmManager(vtkDsmManager*);
  vtkGetObjectMacro(DsmManager, vtkDsmManager);

  // Description:
  // Specify the domain name under which the grid/grids will be written
  vtkSetStringMacro(DomainName);
  vtkGetStringMacro(DomainName);

  // Description:
  // When AppendMode is Temporal, the TimeStep will be used
  // to generate additional file/datagroup/array names
  // to prevent the same name being used twice in any datasets
  vtkSetMacro(TimeStep, int);
  vtkGetMacro(TimeStep, int);

  // Description:
  // Setting GeometryConstant to true tells the writer that all
  // datasets will share the Geometry (points) of the first timestep
  vtkSetMacro(GeometryConstant,int);
  vtkGetMacro(GeometryConstant,int);
  vtkBooleanMacro(GeometryConstant,int);

  // Description:
  // Setting TopologyConstant to true tells the writer that all
  // datasets will share the Topology (Connectivity) of the first timestep.
  vtkSetMacro(TopologyConstant,int);
  vtkGetMacro(TopologyConstant,int);
  vtkBooleanMacro(TopologyConstant,int);

  // Description:
  // Setting TemporalCollection to true tells the writer that all
  // grids will be included in a temporal collection
  // (it is enabled by default)
  vtkSetMacro(TemporalCollection,int);
  vtkGetMacro(TemporalCollection,int);
  vtkBooleanMacro(TemporalCollection,int);

  void CloseHDFFile();

protected:
  vtkXdmfWriter4();
  virtual ~vtkXdmfWriter4();
  //

//BTX
  // Description:
  // Set the build mode for the build function for either write
  // only the xml file/the heavy dataset file or the both
  vtkSetMacro(BuildMode,int);
  vtkGetMacro(BuildMode,int);
  void SetBuildModeToLight();
  void SetBuildModeToHeavy();
  void SetBuildModeToAll();

  // Description:
  // Set the build mode to avoid prevent writing arrays and only opening/closing files
  vtkSetMacro(DummyBuild,int);
  vtkGetMacro(DummyBuild,int);
  vtkBooleanMacro(DummyBuild,int);

  // Description:
  // Takes an xdmf filename and reads the contents into an xdmf DOM structure
  // this structure can be used with other function to create/combine grids
  // into temporal/multiblock collections.
  XdmfDOM  *ParseExistingFile(const char* filename);

  // Description:
  // Takes a dataset as input and creates the XML description for it
  // as an xdmf DOM. The HDF5 files associated with the data are written
  // to disk. The xml is returned and must be written separately.
  XdmfDOM *CreateXdmfGrid(
    vtkDataSet *dataset, const char *name, int index, double time, vtkXW2NodeHelp *staticnode);
//  void BuildHeavyXdmfGrid(vtkMultiBlockDataSet *mbdataset, int data_block, int nb_arrays);

  // Description:
  // Create a single Grid inside a DOM. the collection type should
  // be either Temporal or Spatial.
  XdmfDOM *CreateEmptyCollection(const char *name, const char *ctype);

  // Description:
  // Add one grid (DOM) to another already existing collection
  XdmfDOM *AddGridToCollection(XdmfDOM *cDOM, XdmfDOM *block);

  XdmfXmlNode GetStaticGridNode(vtkDataSet *dsinput, bool multiblock, XdmfDOM *DOM, const char *name, bool &staticFlag);

  vtkstd::string MakeGridName(vtkDataSet *dataset, const char *name, int index);

  // Description:
  // Writes the current data (dataset or multiblock) out to Xdmf xml
  // Note that for simplicity, even a single dataset is written as
  // a temporal collection with just one step.
  void WriteOutputXML(XdmfDOM *outputDOM, XdmfDOM *timestep, double time);

//ETX

  virtual int  FillInputPortInformation(int port, vtkInformation* info);
  virtual int  FillOutputPortInformation(int port, vtkInformation* info);

  virtual int RequestData(
    vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* vtkNotUsed(outputVector));

  virtual void SetXdmfArrayCallbacks(XdmfArray *data);

  //
  // User Variables
  //
  int     CollectiveIO;
  int     AppendMode;
  int     TimeStep;
  char   *DomainName;
  int     TopologyConstant;
  int     GeometryConstant;
  int     TemporalCollection;
  int     BuildMode;
  int     DummyBuild;

  // Used for Parallel write
  int     UpdatePiece;
  int     UpdateNumPieces;

  // Used for DSM write
  vtkDsmManager *DsmManager;

  //
  // Internal Variables
  //
//BTX
  vtkstd::string WorkingDirectory;
  vtkstd::string BaseFileName;
  vtkstd::string XdmfFileName;
  H5MBCallback  *Callback;
//ETX

  vtkMultiProcessController* Controller;

private:
  vtkXdmfWriter4(const vtkXdmfWriter4&);  // Not implemented.
  void operator=(const vtkXdmfWriter4&);  // Not implemented.
};

#endif
