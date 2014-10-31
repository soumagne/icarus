/*=========================================================================

  Project                 : Icarus
  Module                  : vtkBonsaiDsmManager.h

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
// .NAME vtkBonsaiDsmManager - Create/Expose a Bonsai DSM to Paraview
// .SECTION Description
// Create/Expose an Xdmf DSM to Paraview

// This class is not needed, we can use a vtkAbstractDsmManager, 
// made this, just in case we need some special function ...

#ifndef __vtkBonsaiDsmManager_h
#define __vtkBonsaiDsmManager_h

#include "vtkAbstractDsmManager.h"       // Base class

// For PARAVIEW_USE_MPI
#include "vtkPVConfig.h"
#ifdef PARAVIEW_USE_MPI
 #include "vtkMPI.h"
 #include "vtkMPIController.h"
 #include "vtkMPICommunicator.h"
#endif

#define VTK_DSM_MANAGER_DEFAULT_NOTIFICATION_PORT 11112

class vtkMultiProcessController;
class RendererData;

class VTK_EXPORT vtkBonsaiDsmManager : public vtkAbstractDsmManager
{
public:
  static vtkBonsaiDsmManager *New();
  vtkTypeMacro(vtkBonsaiDsmManager,vtkAbstractDsmManager);

  // Description:
  // Make the DSM manager listen for new incoming connection (called by server).
  virtual int Publish();

  // Description:
  // Wait for a notification - notifications are used to trigger user
  // defined tasks and are sent when the dsm has been unlocked after new data is ready
  virtual bool PollingBonsai(unsigned int *flag);

  static bool WaitForNewData(const bool quickSync,
    const int rank, const int nrank);

  static bool fetchSharedData(const bool quickSync, RendererData *rData,
    const int rank, const int nrank, const MPI_Comm &comm,
    const int reduceDM = 1, const int reduceS = 1);

protected:
    vtkBonsaiDsmManager();
    virtual ~vtkBonsaiDsmManager();

    int ThreadActive;

private:
    vtkBonsaiDsmManager(const vtkBonsaiDsmManager&);  // Not implemented.
    void operator=(const vtkBonsaiDsmManager&);  // Not implemented.
};

#endif
