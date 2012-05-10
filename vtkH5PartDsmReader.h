/*=========================================================================

  Project                 : Icarus
  Module                  : vtkH5PartDsmReader.h

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
// .NAME vtkH5PartDsmReader - Read H5Part (HDF5) Particle files from DSM
// .SECTION Description - Read H5Part (HDF5) Particle files from DSM

#ifndef __vtkH5PartDsmReader_h
#define __vtkH5PartDsmReader_h

#include "vtkH5PartReader.h"

class vtkDsmManager;

class VTK_EXPORT vtkH5PartDsmReader : public vtkH5PartReader
{
public:
  static vtkH5PartDsmReader *New();
  vtkTypeMacro(vtkH5PartDsmReader,vtkH5PartReader);
  void PrintSelf(ostream& os, vtkIndent indent);   

  // Description:
  // Set/Get DsmBuffer Manager and enable DSM data reading instead of using
  // disk. The DSM manager is assumed to be created and initialized before
  // being passed into this routine
  virtual void SetDsmManager(vtkDsmManager*);
  vtkGetObjectMacro(DsmManager, vtkDsmManager);


protected:
   vtkH5PartDsmReader();
  ~vtkH5PartDsmReader();
  //
  virtual int  OpenFile();
  virtual void CloseFile();
  virtual void CloseFileIntermediate();
  //
  int ProcessRequest(vtkInformation *request,
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector);

  int RequestInformation(
    vtkInformation *request,
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector);
  //
  vtkDsmManager *DsmManager;
  bool           UsingCachedHandle;
  double         ReadTime;

  //BTX
  H5PartFile *H5Part_open_file_dsm();
  //ETX

private:
  vtkH5PartDsmReader(const vtkH5PartDsmReader&);  // Not implemented.
  void operator=(const vtkH5PartDsmReader&);  // Not implemented.
};

#endif
