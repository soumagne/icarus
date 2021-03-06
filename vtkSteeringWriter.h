/*=========================================================================

  Project                 : Icarus
  Module                  : vtkSteeringWriter.h

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
// .NAME vtkSteeringWriter - Write H5Part (HDF5) Particle files
// .SECTION Description
// vtkSteeringWriter writes compatible with H5Part : documented here
// http://amas.web.psi.ch/docs/H5Part-doc/h5part.html 

#ifndef __vtkSteeringWriter_h
#define __vtkSteeringWriter_h

#include "vtkSmartPointer.h" // For vtkSmartPointer
#include "vtkAbstractParticleWriter.h"
//
#include <hdf5.h>
#include <string>
#include <vector>
//
class vtkMultiProcessController;
//
class vtkDsmManager;
class vtkPointSet;
class vtkCellArray;

class VTK_EXPORT vtkSteeringWriter : public vtkAbstractParticleWriter
{
public:
  static vtkSteeringWriter *New();
  vtkTypeRevisionMacro(vtkSteeringWriter,vtkAbstractParticleWriter);
  void PrintSelf(ostream& os, vtkIndent indent);   

  // Description:
  // Get the input to this writer.
  vtkPointSet* GetInput();
  vtkPointSet* GetInput(int port);

  // Description:
  // Make this public so that files can be closed between time steps and
  // the file might survive an application crash.
  void CloseFile();

  // Description:
  // Set/Get DsmBuffer Manager and enable DSM data writing instead of using
  // disk. The DSM manager is assumed to be created and initialized before
  // being passed into this routine
  virtual void SetDsmManager(vtkDsmManager*);
  vtkGetObjectMacro(DsmManager, vtkDsmManager);

  // Description:
  // Set/Get the controller used for coordinating parallel writing
  // (set to the global controller by default)
  // If not using the default, this must be called before any
  // other methods.
  virtual void SetController(vtkMultiProcessController* controller);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);

  // Description:
  // Set/get a special string generated by pqDataExportWidget/vtkSMDataExportDomain
  // which encode what should be written.
  vtkSetStringMacro(WriteDescription);
  vtkGetStringMacro(WriteDescription);
  

  // Description:
  // Set/get the type of array to write,
  // 0 is Points
  // 1 is connectivity
  // 2 is field array of scalars/vectors
  // if 2 is set, you must also set ArrayName and FieldType (0 point, 1 cell)
  vtkSetMacro(ArrayType, int);
  vtkGetMacro(ArrayType, int);
  vtkSetMacro(FieldType, int);
  vtkGetMacro(FieldType, int);

  // Description:
  // Set/get the name of the array to write, not needed if writing points/connectivity
  vtkSetStringMacro(ArrayName);
  vtkGetStringMacro(ArrayName);

  // Description:
  // Set/get the Group Path that is being written to
  vtkSetStringMacro(GroupPath);
  vtkGetStringMacro(GroupPath);

  // Description:
  // Set/get the Group Path that is being written to
  vtkSetMacro(WriteFloatAsDouble,int);
  vtkGetMacro(WriteFloatAsDouble,int);

  // Override superclass' Write method and make public
  virtual void WriteData();

protected:
  vtkSteeringWriter();
  virtual ~vtkSteeringWriter();
  //
  int OpenFile();
  int OpenGroup(const char *pathwithoutname);
  void CloseGroup(hid_t gid);


  void CopyFromVector(int offset, vtkDataArray *source, vtkDataArray *dest);
  void H5WriteDataArray(hid_t mem_space, hid_t file_space, hsize_t mem_type, hid_t group_id, const char *array_name, vtkDataArray *dataarray, bool convert=false);
  void WriteDataArray(const char *name, vtkDataArray *indata, 
    bool store_offsets, std::vector<vtkIdType> &parallelOffsets);
  void WriteConnectivityTriangles(vtkCellArray *cells, std::vector<vtkIdType> &parallelOffsets);

  // Overide information to only permit PolyData as input
  virtual int FillInputPortInformation(int, vtkInformation *info);
  virtual int FillOutputPortInformation(int, vtkInformation* info);

  // Description:
  virtual int RequestInformation(vtkInformation* request,
      vtkInformationVector** inputVector,
      vtkInformationVector* outputVector);

//BTX
  bool GatherDataArrayInfo(vtkDataArray *data, int &datatype, int &numComponents);
//ETX

  //
  // Internal Variables
  //
  int     ArrayType;
  int     FieldType;
  char   *ArrayName;
  char   *WriteDescription;
  int     WriteFloatAsDouble;

  long long NumberOfParticles;
  hid_t H5FileId;
  hid_t H5GroupId;
  char *GroupPath;

  // used internally
  std::string GroupPathInternal;
  std::string ArrayNameInternal;
  int         ArrayTypeInternal;

  // Used for Parallel write
  int     UpdatePiece;
  int     UpdateNumPieces;

  vtkDsmManager *DsmManager;
  vtkMultiProcessController *Controller;

private:
  vtkSteeringWriter(const vtkSteeringWriter&);  // Not implemented.
  void operator=(const vtkSteeringWriter&);  // Not implemented.
};

#endif
