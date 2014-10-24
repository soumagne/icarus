/*=========================================================================

  Project                 : Icarus
  Module                  : vtkAbstractDsmManager.h

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
// .NAME vtkAbstractDsmManager - Create/Expose an Xdmf DSM to Paraview
// .SECTION Description
// Create/Expose an Xdmf DSM to Paraview

#ifndef __vtkAbstractDsmManager_h
#define __vtkAbstractDsmManager_h

#include "vtkObject.h"       // Base class

#define VTK_DSM_MANAGER_DEFAULT_NOTIFICATION_PORT 11112

class vtkMultiProcessController;

class VTK_EXPORT vtkAbstractDsmManager : public vtkObject
{
public:
  static vtkAbstractDsmManager *New();
  vtkTypeMacro(vtkAbstractDsmManager,vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Set/Get the size of the buffer to be reserved on this process
  // the DSM total size will be the sum of the local sizes from all processes
  virtual void SetLocalBufferSizeMBytes(int size);
  virtual int  GetLocalBufferSizeMBytes();

  // Description:
  // Wait for a notification - notifications are used to trigger user
  // defined tasks and are sent when the file has been unlocked
  virtual int  WaitForUnlock(unsigned int *flag);

  // Description:
  // Signal/Wait for the pipeline update to be finished (only valid when
  // a new notification has been received)
  virtual void SignalUpdated();
  virtual void WaitForUpdated();

  // Create a new DSM buffer of type DsmType using a local length of
  // LocalBufferSizeMBytes and the given MpiComm.
  virtual int Create();

  // Description:
  // Destroy the current DSM buffer.
  virtual int Destroy();

  // Description:
  // Connect to a remote DSM manager (called by client).
  virtual int Connect();

  // Description:
  // Disconnect the manager from the remote end (called by client and server).
  virtual int Disconnect();

  // Description:
  // Make the DSM manager listen for new incoming connection (called by server).
  virtual int Publish();

  // Description:
  // Stop the listening service (called by server).
  virtual int Unpublish();

  // Description:
  // Open the DSM from all nodes of the parallel application, 
  // Currently this is for paraview use and uses H5F_ACC_RDONLY
  // but R/W support can be added if necessary
  // returns 1 on success, 0 on fail
  virtual int OpenCollective();

  // Description:
  // Close the DSM from all nodes of the parallel application, 
  // returns 1 on success, 0 on fail
  virtual int CloseCollective();

  virtual void UpdateSteeredObjects();

  // Description:
  // Set the current given steering command.
  // The command is then passed to the simulation.
  virtual void SetSteeringCommand(char *command);

  // Description:
  // Set values and associated name for the corresponding int interaction.
  virtual void SetSteeringValues(const char *name, int numberOfElements, int *values);

  // Description:
  // Set values and associated name for the corresponding int interaction.
  virtual void SetSteeringValues(const char *name, int numberOfElements, double *values);

  // Description:
  // Get values and associated name for the corresponding int interaction.
  virtual int GetSteeringValues(const char *name, int numberOfElements, int *values);

  // Description:
  // Get values and associated name for the corresponding double interaction.
  virtual int GetSteeringValues(const char *name, int numberOfElements, double *values);

  // Description:
  // The helper proxy used to generate (steering) panel controls must be created
  // on the server, so we pass the string from the client to the DSM manager and
  // let it do the registration and creation of the proxy.
  virtual void SetHelperProxyXMLString(const char *xmlstring);
  vtkGetStringMacro(HelperProxyXMLString);

  // Description:
  // Set/Get the controller use in compositing (set to
  // the global controller by default)
  // If not using the default, this must be called before any
  // other methods.
  virtual void SetController(vtkMultiProcessController* controller);
  vtkGetObjectMacro(Controller, vtkMultiProcessController);

//BTX
    // Registers our XML for the auto-generated steering proxy
    // with the proxy manager. 
    // We need to register on the server side as well as client.
    static void RegisterHelperProxy(const char *xmlstring);
//ETX

protected:
    vtkAbstractDsmManager();
    virtual ~vtkAbstractDsmManager();

    //
    // Internal Variables
    //
    int            UpdatePiece;
    int            UpdateNumPieces;

    // If the user is running paraview client in stand-alone mode and not
    // an mpijob, the mpi controller will be a vtkDummyController
    // check for this and replace with an MPI controller if necessary when
    // first setting up a DSM object
    virtual void CheckMPIController();

    vtkMultiProcessController *Controller;
    //
    char *HelperProxyXMLString;

private:
    vtkAbstractDsmManager(const vtkAbstractDsmManager&);  // Not implemented.
    void operator=(const vtkAbstractDsmManager&);  // Not implemented.
};

#endif
