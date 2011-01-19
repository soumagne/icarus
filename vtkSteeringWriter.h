/*=========================================================================

  Project                 : vtkCSCS
  Module                  : vtkSteeringWriter.h
  Revision of last commit : $Rev: 153 $
  Author of last commit   : $Author: biddisco $
  Date of last commit     : $Date:: 2006-07-12 10:09:37 +0200 #$

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
// .NAME vtkSteeringWriter - Write H5Part (HDF5) Particle files
// .SECTION Description
// vtkSteeringWriter writes compatible with H5Part : documented here
// http://amas.web.psi.ch/docs/H5Part-doc/h5part.html 

#ifndef __vtkSteeringWriter_h
#define __vtkSteeringWriter_h

#include "vtkToolkits.h"     // For VTK_USE_MPI 
#include "vtkSmartPointer.h" // For vtkSmartPointer
#include <vtkstd/string>     // for strings
#include <vtkstd/vector>     // for vectors
#include "vtkAbstractParticleWriter.h"
//
#include <hdf5.h>
//
#ifdef VTK_USE_MPI
class vtkMultiProcessController;
#endif
//
class vtkDSMManager;
class vtkPointSet;
class vtkDataArray;
class vtkPointData;

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
  virtual void SetDSMManager(vtkDSMManager*);
  vtkGetObjectMacro(DSMManager, vtkDSMManager);

  //BTX
#ifdef VTK_USE_MPI
  // Description:
  // Set/Get the controller used for coordinating parallel writing
  // (set to the global controller by default)
  // If not using the default, this must be called before any
  // other methods.
  virtual void SetController(vtkMultiProcessController* controller);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);
#endif
  //ETX

  // Description:
  // Set/get the Group Path that is being written to
  vtkSetStringMacro(GroupPath);
  vtkGetStringMacro(GroupPath);

protected:
  vtkSteeringWriter();
  ~vtkSteeringWriter();
  //
  int   OpenFile();

  // Override superclass' Write method
  virtual void WriteData();

  void CopyFromVector(int offset, vtkDataArray *source, vtkDataArray *dest);
  void H5WriteDataArray(hid_t mem_space, hid_t file_space, hsize_t mem_type, hid_t group_id, const char *array_name, vtkDataArray *dataarray);
  void WriteDataArray(int i, vtkDataArray *array);

  // Overide information to only permit PolyData as input
  virtual int FillInputPortInformation(int, vtkInformation *info);
  virtual int FillOutputPortInformation(int, vtkInformation* info);

  // Description:
  virtual int RequestInformation(vtkInformation* request,
      vtkInformationVector** inputVector,
      vtkInformationVector* outputVector);

  //
  // Internal Variables
  //
  long long NumberOfParticles;
  hid_t H5FileId;
  hid_t H5GroupId;
  char *GroupPath;

  // Used for Parallel write
  int     UpdatePiece;
  int     UpdateNumPieces;

  //BTX
#ifdef VTK_USE_MPI
  //ETX
  vtkDSMManager *DSMManager;
  vtkMultiProcessController* Controller;
  //BTX
#endif
  //ETX

private:
  vtkSteeringWriter(const vtkSteeringWriter&);  // Not implemented.
  void operator=(const vtkSteeringWriter&);  // Not implemented.
};

#endif
