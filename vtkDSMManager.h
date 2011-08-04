/*=========================================================================

  Project                 : Icarus
  Module                  : vtkDSMManager.h

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
// .NAME vtkDSMManager - Create/Expose an Xdmf DSM to Paraview
// .SECTION Description
// Create/Expose an Xdmf DSM to Paraview

#ifndef __vtkDSMManager_h
#define __vtkDSMManager_h

#include "vtkToolkits.h"     // For VTK_USE_MPI 
#include "vtkObject.h"       // Base class

#include "H5FDdsmManager.h"

class vtkMultiProcessController;

class VTK_EXPORT vtkDSMManager : public vtkObject
{
public:
  static vtkDSMManager *New();
  vtkTypeRevisionMacro(vtkDSMManager,vtkObject);
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
  // Set/Get the published host name of our connection.
  // Real value valid after a PublishDSM call has been made.
  void SetServerHostName(const char* serverHostName) { DsmManager->SetServerHostName(serverHostName); }
  const char *GetServerHostName() { return(DsmManager->GetServerHostName()); }

  // Description:
  // Set/Get the published port of our connection.
  // Real value valid after a PublishDSM call has been made.
  void SetServerPort(int port) { DsmManager->SetServerPort(port); }
  int  GetServerPort() { return(DsmManager->GetServerPort()); }

  // Description:
  // Only valid after a PublishDSM call has been made.
  int GetIsConnected() { return(DsmManager->GetIsConnected()); }
  int WaitForConnection() { return(DsmManager->WaitForConnection()); }

  // Description:
  // Wait for a notification - notifications are used to trigger user
  // defined tasks and are usually sent once the file has been closed
  // but can also be sent on demand.
  int  GetIsNotified() { return(DsmManager->GetIsNotified()); }
  void ClearIsNotified() { DsmManager->ClearIsNotified(); }
  int  WaitForNotification() { return(DsmManager->WaitForNotification()); }
  void NotificationFinalize() { DsmManager->NotificationFinalize(); }

  // Description:
  // Get the notification flag - Only valid if GetDsmIsNotified is true.
  int  GetNotification() { return(DsmManager->GetNotification()); }
  void ClearNotification() { DsmManager->ClearNotification(); }

  // Description:
  // Get/Set the display update flag which triggers the update the view
  int  GetIsDataModified() { return(DsmManager->GetIsDataModified()); }
  void ClearIsDataModified() { DsmManager->ClearIsDataModified(); }

  // Description:
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
  // When using static inter-communicators, create a new inter-communicator
  // linking both managers together (called by client and server).
  int ConnectInterComm() { return(DsmManager->ConnectInterComm()); }

  // Description:
  // Disconnect the manager from the remote end (called by client and server).
  int Disconnect() { return(DsmManager->Disconnect()); }

  // Description:
  // Make the DSM manager listen for new incoming connection (called by server).
  int Publish() { return(DsmManager->Publish()); }

  // Description:
  // Stop the listening service (called by server).
  int Unpublish() { return(DsmManager->Unpublish()); }

  // Description:
  // Dump out the content of the DSM buffer (complete output).
  void H5Dump() { DsmManager->H5Dump(); }

  // Description:
  // Dump out the content of the DSM buffer (hierarchical output).
  void H5DumpLight() { DsmManager->H5DumpLight(); }

  // Description:
  // Dump out the content of the DSM buffer (XML output).
  void H5DumpXML() { DsmManager->H5DumpXML(); }

  // Description:
  // Generate an Xdmf description file.
  void GenerateXMFDescription();

  // Description:
  // (Debug) Send an XML string.
  void SendDSMXML() { DsmManager->SendDSMXML(); }

  // Description:
  // When sending, the writer can SetXMLDescriptionSend and it will be transmitted
  // to the receiver. When receiving, GetXMLDescriptionReceive queries the internal DSMBuffer
  // object to see if a string is present
  void SetXMLStringSend(const char *XMLStringSend) { DsmManager->SetXMLStringSend(XMLStringSend); }
  const char *GetXMLStringReceive() { return(DsmManager->GetXMLStringReceive()); }
  void        ClearXMLStringReceive() { DsmManager->ClearXMLStringReceive(); }

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
  // Set/Get the file path pointing either to an XDMF description file
  // or to the XDMF template used to generate a proper XDMF file.
  vtkSetStringMacro(XMFDescriptionFilePath);
  vtkGetStringMacro(XMFDescriptionFilePath);

//BTX
  #ifdef VTK_USE_MPI
//ETX
//BTX
    // Description:
    // Set/Get the controller use in compositing (set to
    // the global controller by default)
    // If not using the default, this must be called before any
    // other methods.
    virtual void SetController(vtkMultiProcessController* controller);
    vtkGetObjectMacro(Controller, vtkMultiProcessController);
//ETX
//BTX
  #endif
//ETX

//BTX
    // Registers our XML for the auto-generated steering proxy
    // with the proxy manager. 
    // We need to register on the server side as well as client.
    static void RegisterHelperProxy(const char *xmlstring);
//ETX

protected:
     vtkDSMManager();
    ~vtkDSMManager();

    //
    // Internal Variables
    //
    int            UpdatePiece;
    int            UpdateNumPieces;

    //BTX
#ifdef VTK_USE_MPI
    // If the user is running paraview client in stand-alone mode and not
    // an mpijob, the mpi controller will be a vtkDummyController
    // check for this and replace with an MPI controller if necessary when
    // first setting up a DSM object
    void CheckMPIController();

    //ETX
    vtkMultiProcessController *Controller;
#endif

    //
    char           *XMFDescriptionFilePath;
    char           *HelperProxyXMLString;
    //BTX
    H5FDdsmManager *DsmManager;
    //ETX

private:
    vtkDSMManager(const vtkDSMManager&);  // Not implemented.
    void operator=(const vtkDSMManager&);  // Not implemented.
};

#endif
