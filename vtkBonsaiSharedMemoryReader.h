/*=========================================================================

  Project                 : pv-meshless
  Module                  : vtkBonsaiSharedMemoryReader.h
  Revision of last commit : $Rev: 754 $
  Author of last commit   : $Author: biddisco $
  Date of last commit     : $Date:: 2009-01-09 13:40:38 +0100 #$

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
// .NAME vtkBonsaiSharedMemoryReader - Write H5Part (HDF5) Particle files
// .SECTION Description
// vtkBonsaiSharedMemoryReader reads compatible with H5Part : documented here
// http://amas.web.psi.ch/docs/H5Part-doc/h5part.html 

#ifndef __vtkBonsaiSharedMemoryReader_h
#define __vtkBonsaiSharedMemoryReader_h

#include "vtkPolyDataAlgorithm.h"
#include "vtkBoundingBox.h"
#include <string>
#include <vector>

class vtkDataArraySelection;
class vtkMultiProcessController;
class vtkBoundsExtentTranslator;

struct H5PartFile;

class VTK_EXPORT vtkBonsaiSharedMemoryReader : public vtkPolyDataAlgorithm
{
public:
  static vtkBonsaiSharedMemoryReader *New();
  vtkTypeMacro(vtkBonsaiSharedMemoryReader,vtkPolyDataAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);   

  // Description:
  // Specify file name.
  void SetFileName(char *filename);
  vtkGetStringMacro(FileName);

  // Description:
  // Set/Get the timestep to be read
  vtkSetMacro(TimeStep,int);
  vtkGetMacro(TimeStep,int);
  
  // Description:
  // Export time values as 0,1...N-1 regardless of real time values in file
  vtkSetMacro(IntegerTimeStepValues,int);
  vtkGetMacro(IntegerTimeStepValues,int);
  vtkBooleanMacro(IntegerTimeStepValues,int);
  
  // Description:
  // Get the number of timesteps in the file
  vtkGetMacro(NumberOfTimeSteps,int);

  // Description:
  // When set (default no), the reader will generate a vertex cell
  // for each point/particle read. When using the points directly
  // this is unnecessary and time can be saved by omitting cell generation
  // vtkPointSpriteMapper does not require them.
  // When using ParaView, cell generation is recommended, without them
  // many filter operations are unavailable
  vtkSetMacro(GenerateVertexCells, int);
  vtkGetMacro(GenerateVertexCells, int);
  vtkBooleanMacro(GenerateVertexCells, int);
  
  // Description:
  // If set and present in the file, bounding boxes of each parallel
  // piece will be exported (as lines) along with particles.
  // Note that the Partition Boxes are the ones present in the file
  // and the Piece Boxes are the ones we export from the reader
  // which will consist of 1 or more partitions joined together.
  vtkSetMacro(DisplayPieceBoxes,int);
  vtkGetMacro(DisplayPieceBoxes,int);
  vtkBooleanMacro(DisplayPieceBoxes,int);
  
  // Description:
  // An H5Part file may contain multiple arrays
  // a GUI (eg Paraview) can provide a mechanism for selecting which data arrays
  // are to be read from the file. The PointArray variables and members can
  // be used to query the names and number of arrays available
  // and set the status (on/off) for each array, thereby controlling which
  // should be read from the file. Paraview queries these point arrays after
  // the (update) information part of the pipeline has been updated, and before the
  // (update) data part is updated.
  int         GetNumberOfPointArrays();
  const char* GetPointArrayName(int index);
  int         GetPointArrayStatus(const char* name);
  void        SetPointArrayStatus(const char* name, int status);
  void        DisableAll();
  void        EnableAll();
  void        Disable(const char* name);
  void        Enable(const char* name);
  //
  int         GetNumberOfPointArrayStatusArrays() { return GetNumberOfPointArrays(); }
  const char* GetPointArrayStatusArrayName(int index) { return GetPointArrayName(index); }
  int         GetPointArrayStatusArrayStatus(const char* name) { return GetPointArrayStatus(name); }
  void        SetPointArrayStatusArrayStatus(const char* name, int status) { SetPointArrayStatus(name, status); }

  // Description:
  // Set/Get the controller use in parallel operations 
  // (set to the global controller by default)
  // If not using the default, this must be set before any
  // other methods.
  virtual void SetController(vtkMultiProcessController* controller);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);

  void SetFileModified();

protected:
   vtkBonsaiSharedMemoryReader();
  ~vtkBonsaiSharedMemoryReader();
  //
  int   RequestInformation(vtkInformation *, vtkInformationVector **, vtkInformationVector *);
  int   RequestData(vtkInformation *, vtkInformationVector **, vtkInformationVector *);

  // read bboxes information into box/partition arrays
  vtkIdType ReadBoundingBoxes();

  // create polygons/lines from boxes for visual display
  // Note : we only colour particles actually read on this process 
  // i.e. those between extent0 and extent1
  vtkIdType DisplayBoundingBoxes(vtkDataArray *coords, vtkPolyData *output, vtkIdType extent0, vtkIdType extent1);

  //
  virtual int  OpenFile();
  virtual void CloseFile();

  //
  // Internal Variables
  //
  char         *FileName;
  int           NumberOfTimeSteps;
  int           TimeStep;
  int           ActualTimeStep;
  double        TimeStepTolerance;
  int           GenerateVertexCells;
  vtkTimeStamp  FileModifiedTime;
  vtkTimeStamp  FileOpenedTime;
  int           UpdatePiece;
  int           UpdateNumPieces;
  int           MaskOutOfTimeRangeOutput;
  int           TimeOutOfRange;
  int           IntegerTimeStepValues;
  int           DisplayPieceBoxes;
  //
//BTX
  std::vector<double>                  TimeStepValues;
  typedef std::vector<std::string>  stringlist;
  std::vector<stringlist>              FieldArrays;
  // For Bounding boxes if present
  std::vector<vtkIdType>      PartitionCount;
  std::vector<vtkIdType>      PartitionOffset;
  std::vector<vtkIdType>      PieceId;
  std::vector<double>         PartitionBoundsTable;
  std::vector<double>         PartitionBoundsTableHalo;
  std::vector<vtkBoundingBox> PieceBounds;
  std::vector<vtkBoundingBox> PieceBoundsHalo;
  vtkBoundsExtentTranslator  *ExtentTranslator;
//ETX

  // To allow paraview gui to enable/disable scalar reading
  vtkDataArraySelection* PointDataArraySelection;

  vtkMultiProcessController* Controller;

private:
  vtkBonsaiSharedMemoryReader(const vtkBonsaiSharedMemoryReader&);  // Not implemented.
  void operator=(const vtkBonsaiSharedMemoryReader&);  // Not implemented.
};

#endif
