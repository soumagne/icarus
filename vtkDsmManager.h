/*=========================================================================

  Project                 : Icarus
  Module                  : vtkDsmManager.h

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
// .NAME vtkDsmManager - Create/Expose an Xdmf DSM to Paraview
// .SECTION Description
// Create/Expose an Xdmf DSM to Paraview

#ifndef __vtkDsmManager_h
#define __vtkDsmManager_h

#include "vtkObject.h"       // Base class

#include "H5FDdsmManager.h"
#include "H5Fpublic.h" // for H5F_ACC_RDONLY etc

#define VTK_DSM_MANAGER_DEFAULT_NOTIFICATION_PORT 11112

class vtkMultiProcessController;

class VTK_EXPORT vtkDsmManager : public vtkObject
{
public:
  static vtkDsmManager *New();
  vtkTypeRevisionMacro(vtkDsmManager,vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  H5FDdsmBuffer *GetDsmBuffer() { return(DsmManager->GetDsmBuffer()); }

  // Description:
  // Set/Get the size of the buffer to be reserved on this process
  // the DSM total size will be the sum of the local sizes from all processes
  void SetLocalBufferSizeMBytes(int size) { DsmManager->SetLocalBufferSizeMBytes(size); }
  int  GetLocalBufferSizeMBytes() { return(DsmManager->GetLocalBufferSizeMBytes()); }

  // Description:
  // Set/Get DsmIsServer info
  void SetIsServer(int isServer) { DsmManager->SetIsServer(isServer); }
  int  GetIsServer() { return(DsmManager->GetIsServer()); }

  // Description:
  // Set/Get the interprocess communication subsystem
  void SetInterCommType(int commType) { DsmManager->SetInterCommType(commType); }
  int  GetInterCommType() { return(DsmManager->GetInterCommType()); }

  // Description:
  // Set the DSM block length when using H5FD_DSM_TYPE_BLOCK_CYCLIC
  void SetBlockLength(int blockLength) { DsmManager->SetBlockLength(blockLength); }

  // Description:
  // Set/Get the DSM distribution type
  void SetDsmType(int type) { DsmManager->SetDsmType(type); }
  int  GetDsmType() { return(DsmManager->GetDsmType()); }

  // Description:
  // Set/Get UseStaticInterComm -- Force to use static MPI comm model
  // when dynamic MPI communication is not supported by the system
  void SetUseStaticInterComm(int value) { DsmManager->SetUseStaticInterComm(value); }
  int  GetUseStaticInterComm() { return(DsmManager->GetUseStaticInterComm()); }

  // Description:
  // Set/Get the published host name of our connection.
  // Real value valid after a Publish call has been made.
  void SetServerHostName(const char* serverHostName) { DsmManager->SetServerHostName(serverHostName); }
  const char *GetServerHostName() { return(DsmManager->GetServerHostName()); }

  // Description:
  // Set/Get the published port of our connection.
  // Real value valid after a Publish call has been made.
  void SetServerPort(int port) { DsmManager->SetServerPort(port); }
  int  GetServerPort() { return(DsmManager->GetServerPort()); }

  // Description:
  // Wait for a connection - Only valid after a Publish call has been made.
  int GetIsConnected() { return(DsmManager->GetIsConnected()); }
  int WaitForConnection() { return(DsmManager->WaitForConnection()); }

  // Description:
  // Wait for a notification - notifications are used to trigger user
  // defined tasks and are sent when the file has been unlocked
  int  WaitForUnlock() { return(DsmManager->WaitForUnlock()); }
  void *NotificationThread();

  // Description:
  // Signal/Wait for the pipeline update to be finished (only valid when
  // a new notification has been received)
  void SignalUpdated();
  void WaitForUpdated();

  // Description:
  // Get the notification flag - Only valid if GetDsmIsNotified is true.
  int  GetNotification() { return(DsmManager->GetUnlockStatus()); }

  // Create a new DSM buffer of type DsmType using a local length of
  // LocalBufferSizeMBytes and the given MpiComm.
  int Create();

  // Description:
  // Destroy the current DSM buffer.
  int Destroy();

  // Description:
  // Clear the DSM storage.
  int ClearStorage() { return(DsmManager->ClearStorage()); }

  // Description:
  // Connect to a remote DSM manager (called by client).
  int Connect() { return(DsmManager->Connect()); }

  // Description:
  // Disconnect the manager from the remote end (called by client and server).
  int Disconnect() { return(DsmManager->Disconnect()); }

  // Description:
  // Make the DSM manager listen for new incoming connection (called by server).
  int Publish();

  // Description:
  // Stop the listening service (called by server).
  int Unpublish() { return(DsmManager->Unpublish()); }

  // Description:
  // Open the DSM from all nodes of the parallel application, 
  // Currently this is for paraview use and uses H5F_ACC_RDONLY
  // but R/W support can be added if necessary
  // returns 1 on success, 0 on fail
  int OpenCollective() { return (DsmManager->OpenDSM(H5F_ACC_RDONLY)); }

  // Description:
  // Open the DSM from all nodes of the parallel application for Read/Write, 
  // returns 1 on success, 0 on fail
  int OpenCollectiveRW() { return (DsmManager->OpenDSM(H5F_ACC_RDWR)); }

  // Description:
  // Close the DSM from all nodes of the parallel application, 
  // returns 1 on success, 0 on fail
  int CloseCollective() { return (DsmManager->CloseDSM()); }

  // Description:
  // Certain ParaView operations open the file on just rank 0
  // to support this we must allow a serial mode so that collectives are 
  // skipped. Use with extreme care.
  void SetSerialMode(int serial) { DsmManager->SetIsDriverSerial(serial); }

  // Description:
  // Dump out the content of the DSM buffer (complete output).
//  void H5Dump() { H5FD_dsm_dump(); }

  // Description:
  // Dump out the content of the DSM buffer (hierarchical output).
//  void H5DumpLight() { H5FD_dsm_dumpLight(); }

  // Description:
  // Dump out the content of the DSM buffer (XML output).
//  void H5DumpXML() { H5FD_dsm_dumpXML(); }

  // Description:
  // Generate an Xdmf description file. The generated file is automatically set
  // to the DSM buffer.
  void GenerateXdmfDescription();

  // Description:
  // Set the Xdmf description file.
  vtkGetStringMacro(XdmfDescription);
  vtkSetStringMacro(XdmfDescription);

  // Description:
  // If the .dsm_client_config file exists in the standard location
  // $ENV{H5FD_DSM_CONFIG_PATH}/.dsm_client_config then the server/port/mode
  // information can be read. This is for use the by a DSM client.
  // DSM servers write their .dsm_client_config when Publish() is called
  // Returns false if the .dsm_client_config file is not read.
  int ReadConfigFile();

  void UpdateSteeredObjects() { DsmManager->UpdateSteeredObjects(); }

  // Description:
  // Set the current given steering command.
  // The command is then passed to the simulation.
  void SetSteeringCommand(char *command) { DsmManager->SetSteeringCommand(command); }

  // Description:
  // Set values and associated name for the corresponding int interaction.
  void SetSteeringValues(const char *name, int numberOfElements, int *values)
  {
    DsmManager->SetSteeringValues(name, numberOfElements, values);
    DsmManager->WriteSteeredData();
  }

  // Description:
  // Set values and associated name for the corresponding int interaction.
  void SetSteeringValues(const char *name, int numberOfElements, double *values)
  {
    DsmManager->SetSteeringValues(name, numberOfElements, values);
    DsmManager->WriteSteeredData();
  }

  // Description:
  // Get values and associated name for the corresponding int interaction.
  int GetSteeringValues(const char *name, int numberOfElements, int *values)
  {
    return(DsmManager->GetSteeringValues(name, numberOfElements, values));
  }

  // Description:
  // Get values and associated name for the corresponding double interaction.
  int GetSteeringValues(const char *name, int numberOfElements, double *values)
  {
    return(DsmManager->GetSteeringValues(name, numberOfElements, values));
  }

  // Description:
  // Return 1 if the Interactions group exists, 0 otherwise
  int GetInteractionsGroupPresent()
  {
    return(DsmManager->GetInteractionsGroupPresent());
  }

  // Description:
  // Set/Unset objects
  void SetDisabledObject(char *objectName) { DsmManager->SetDisabledObject(objectName); }

  // Description:
  // The helper proxy used to generate (steering) panel controls must be created
  // on the server, so we pass the string from the client to the DSM manager and
  // let it do the registration and creation of the proxy.
  void SetHelperProxyXMLString(const char *xmlstring);
  vtkGetStringMacro(HelperProxyXMLString);

//BTX
  // Description:
  // Get the associated H5FDdsmManager
  vtkGetMacro(DsmManager, H5FDdsmManager*);
//ETX

  // Description:
  // Set/Get the XdmfTemplateDescription file, this can be a path
  // or a string containing the description.
  vtkSetStringMacro(XdmfTemplateDescription);
  vtkGetStringMacro(XdmfTemplateDescription);

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
    vtkDsmManager();
    virtual ~vtkDsmManager();

    //
    // Internal Variables
    //
    int            UpdatePiece;
    int            UpdateNumPieces;

    // If the user is running paraview client in stand-alone mode and not
    // an mpijob, the mpi controller will be a vtkDummyController
    // check for this and replace with an MPI controller if necessary when
    // first setting up a DSM object
    void CheckMPIController();

    vtkMultiProcessController *Controller;

    //
    char           *XdmfTemplateDescription;
    char           *XdmfDescription;
    char           *HelperProxyXMLString;
    //BTX
    H5FDdsmManager *DsmManager;
    //ETX

    //BTX
    struct vtkDsmManagerInternals;
    vtkDsmManagerInternals *DsmManagerInternals;
    //ETX
private:
    vtkDsmManager(const vtkDsmManager&);  // Not implemented.
    void operator=(const vtkDsmManager&);  // Not implemented.
};

#endif
