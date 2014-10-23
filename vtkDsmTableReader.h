/*=========================================================================

  Project                 : Icarus
  Module                  : vtkDsmTableReader.h

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
// .NAME vtkDsmTableReader - Read H5Part (HDF5) Particle files from DSM
// .SECTION Description - Read H5Part (HDF5) Particle files from DSM

#ifndef __vtkDsmTableReader_h
#define __vtkDsmTableReader_h

#include "vtkTableAlgorithm.h"
#include "vtkSmartPointer.h" // for memory safety
#include "vtkStringList.h"
#include "IcarusConfig.h"
#include <string>
#include <vector>

class vtkPoints;
class vtkCellArray;
class vtkDoubleArray;
class vtkPointData;

class vtkHDF5DsmManager;

class VTK_EXPORT vtkDsmTableReader : public vtkTableAlgorithm
{
public:
  static vtkDsmTableReader *New();
  vtkTypeMacro(vtkDsmTableReader,vtkTableAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);   

  virtual void SetNameStrings(vtkStringList*);
  vtkGetObjectMacro(NameStrings, vtkStringList);

  // Description:
  // Flush will wipe any existing data so that traces can be restarted from
  // whatever time step is next supplied.
  void Flush();

  // Description:
  // Set/Get DsmBuffer Manager and enable DSM data reading instead of using
  // disk. The DSM manager is assumed to be created and initialized before
  // being passed into this routine
  virtual void SetDsmManager(vtkHDF5DsmManager*);
  vtkGetObjectMacro(DsmManager, vtkHDF5DsmManager);

  void SetFileModified();

protected:
   vtkDsmTableReader();
  ~vtkDsmTableReader();
  //
  virtual int  OpenFile();
  virtual void CloseFile();
  //
  int ProcessRequest(vtkInformation *request,
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector);
  //
  int RequestInformation(
    vtkInformation *request,
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector);
  //
  virtual int RequestData(vtkInformation *request,
                          vtkInformationVector** inputVector,
                          vtkInformationVector* outputVector);
  //
  vtkHDF5DsmManager  *DsmManager;
  vtkStringList  *NameStrings;
  vtkTimeStamp    FileModifiedTime;
  vtkTimeStamp    FileOpenedTime;

  // internal data variables
  int           NumberOfTimeSteps;
  double        LatestTime;
  //
//BTX
  vtkSmartPointer<vtkPointData>   Values;
  vtkSmartPointer<vtkDoubleArray> TimeData;
  std::vector< vtkSmartPointer<vtkDoubleArray> > DataArrays;
//ETX

private:
  vtkDsmTableReader(const vtkDsmTableReader&);  // Not implemented.
  void operator=(const vtkDsmTableReader&);  // Not implemented.
};

#endif
