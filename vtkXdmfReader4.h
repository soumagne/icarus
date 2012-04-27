/*=========================================================================

  Project                 : Icarus
  Module                  : vtkXdmfReader4.h

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
// .NAME vtkXdmfReader4 - Read Xdmf + HDF5 files using DSM manager
// .SECTION Description
// This reader reads data using the conventional vtkXdmreader but adds DSM
// specific calls in order to use the DSM manager.

#ifndef __vtkXdmfReader4_h
#define __vtkXdmfReader4_h

#include "vtkXdmfReader.h"

class vtkMultiProcessController;
class vtkDsmManager;

class VTK_EXPORT vtkXdmfReader4 : public vtkXdmfReader
{
public:
  static vtkXdmfReader4 *New();
  vtkTypeMacro(vtkXdmfReader4,vtkXdmfReader);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Set/Get the controller used for coordinating parallel reading
  // (set to the global controller by default)
  // If not using the default, this must be called before any
  // other methods.
  virtual void SetController(vtkMultiProcessController* controller);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);

  // Description:
  // Set/Get DsmBuffer Manager and enable DSM data reading instead of using
  // disk. The DSM manager is assumed to be created and initialized before
  // being passed into this routine
  virtual void SetVtkDsmManager(vtkDsmManager*);
  vtkGetObjectMacro(VtkDsmManager, vtkDsmManager);

  // Description:
  // Get the time range for the simulation run
  // provided to enable DSM based simulations to declare a range
  vtkGetVector2Macro(TimeRange, double);
  vtkSetVector2Macro(TimeRange, double);

protected:
  vtkXdmfReader4();
  virtual ~vtkXdmfReader4();
  //
  bool PrepareDsmManagerDocument();

  int ProcessRequest(vtkInformation *request,
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector);

  int RequestInformation(
    vtkInformation *request,
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector);

  //
  // User Variables
  //
  vtkMultiProcessController* Controller;

  // Used for DSM write
  vtkDsmManager *VtkDsmManager;
  double         TimeRange[2];

private:
  vtkXdmfReader4(const vtkXdmfReader4&);  // Not implemented.
  void operator=(const vtkXdmfReader4&);  // Not implemented.
};

#endif
