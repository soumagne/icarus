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

  // Description:
  // Set/Get the size of the buffer to be reserved on this process
  // the DSM total size will be the sum of the local sizes from all processes
  void SetLocalBufferSizeMBytes(int size) { DsmManager->SetLocalBufferSizeMBytes(size); }
  int GetLocalBufferSizeMBytes() { return DsmManager->GetLocalBufferSizeMBytes(); }

  // Description:
  // Set/Get DsmIsServer info
  void SetDsmIsServer(int isServer) { DsmManager->SetDsmIsServer(isServer); }
  int GetDsmIsServer() { return DsmManager->GetDsmIsServer(); }

  // Description:
  // Set/Get the interprocess communication subsystem
  void SetDsmCommType(int type) { DsmManager->SetDsmCommType(type); }
  int GetDsmCommType() { return DsmManager->GetDsmCommType(); }

  // Description:
  // Set/Get the published host name of our connection.
  // Real value valid after a PublishDSM call has been made.
  void SetServerHostName(const char* serverHostName) { DsmManager->SetServerHostName(serverHostName); }
  const char *GetServerHostName() { return DsmManager->GetServerHostName(); }

  // Description:
  // Set/Get the published port of our connection.
  // Real value valid after a PublishDSM call has been made.
  void SetServerPort(int port) { DsmManager->SetServerPort(port); }
  int GetServerPort() { return DsmManager->GetServerPort(); }

  // Description:
  // Only valid after a AcceptConnection call has been made.
  int GetAcceptedConnection() { return DsmManager->GetAcceptedConnection(); }

  // Description:
  // Get/Set the update ready flag which triggers the VTK pipeline update.
  int GetDsmUpdateReady() { return DsmManager->GetDsmUpdateReady(); }
  void ClearDsmUpdateReady() { return DsmManager->ClearDsmUpdateReady(); }

  // Description:
  // Get/Set the display update flag which triggers the update the view
  int  GetDsmUpdateDisplay() { return DsmManager->GetDsmIsDataModified(); }
  void ClearDsmUpdateDisplay() { DsmManager->ClearDsmIsDataModified(); }

  // Description:
  // Set the current given steering command.
  // The command is then passed to the simulation.
  void SetSteeringCommand(char *command) { DsmManager->SetSteeringCommand(command); }

  // Description:
  // Set values and associated name for the corresponding int interaction.
  void SetSteeringValues(const char *name, int numberOfElements, int *values)
  {
    DsmManager->SetSteeringValues(name, numberOfElements, values);
  }

  // Description:
  // Set values and associated name for the corresponding int interaction.
  void SetSteeringValues(const char *name, int numberOfElements, double *values)
  {
    DsmManager->SetSteeringValues(name, numberOfElements, values);
  }

  // Description:
  // Get values and associated name for the corresponding int interaction.
  void GetSteeringValues(const char *name, int numberOfElements, int *values)
  {
    DsmManager->GetSteeringValues(name, numberOfElements, values);
  }

  // Description:
  // Set values and associated name for the corresponding int interaction.
  void GetSteeringValues(const char *name, int numberOfElements, double *values)
  {
    DsmManager->GetSteeringValues(name, numberOfElements, values);
  }

  // Description:
  // Set/Unset objects
  void SetDisabledObject(char *objectName) { DsmManager->SetDisabledObject(objectName); }

  // Description:
  // When sending, the writer can SetXMLDescriptionSend and it will be transmitted
  // to the receiver. When receiving, GetXMLDescriptionReceive queries the internal DSMBuffer
  // object to see if a string is present
  void SetXMLStringSend(const char *XMLStringSend) { DsmManager->SetXMLStringSend(XMLStringSend); }
  const char *GetXMLStringReceive() { return DsmManager->GetXMLStringReceive(); }
  void        ClearXMLStringReceive() { DsmManager->ClearXMLStringReceive(); }

  // Description:
  // The helper proxy used to generate (steering) panel controls must be created
  // on the server, so we pass the string from the client to the DSM manager and
  // let it do the registration and creation of the proxy.
  void SetHelperProxyXMLString(const char *xmlstring);
  vtkGetStringMacro(HelperProxyXMLString);

  // Description:
  // Get the associated DSM buffer handle
  H5FDdsmBuffer *GetDSMHandle() { return DsmManager->GetDSMHandle(); }

  // Description:
  // Get the associated H5FDdsmManager
  vtkGetMacro(DsmManager, H5FDdsmManager*);

  // Description:
  // Set/Get the file path pointing either to an XDMF description file
  // or to the XDMF template used to generate a proper XDMF file.
  vtkSetStringMacro(XMFDescriptionFilePath);
  vtkGetStringMacro(XMFDescriptionFilePath);

  bool   CreateDSM();
  bool   DestroyDSM();
  void   ClearDSM() { DsmManager->ClearDSM(); }
  void   ConnectDSM() { DsmManager->ConnectDSM(); }
  void   DisconnectDSM() { DsmManager->DisconnectDSM(); }
  void   PublishDSM() { DsmManager->PublishDSM(); }
  void   UnpublishDSM() { DsmManager->UnpublishDSM(); }
  void   H5Dump() { DsmManager->H5Dump(); }
  void   H5DumpLight() {  DsmManager->H5DumpLight(); }
  void   H5DumpXML() { DsmManager->H5DumpXML(); }
  void   GenerateXMFDescription();
  void   SendDSMXML() { DsmManager->SendDSMXML(); }
  void   RequestRemoteChannel() { DsmManager->RequestRemoteChannel(); }

  // Description:
  // If the .dsm_config file exists in the standard location
  // $ENV{DSM_CONFIG_PATH}/.dsm_config or in the location set by
  // @CMAKE{H5FDdsm_CONFIG_PATH}/.dsm_config then the server/port/mode
  // information can be read. This is for use the by a DSM client.
  // DSM servers write their .dsm_config when PublishDSM() is called
  // Returns false if the .dsm_config file is not read
  bool   ReadDSMConfigFile();

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
