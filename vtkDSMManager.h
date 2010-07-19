/*=========================================================================

  Project                 : vtkCSCS
  Module                  : vtkDSMManager.h

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
  // Get/Set the update ready flag which triggers the VTK pipeline update and the
  // display of DSM objects.
  void SetDsmUpdateReady(int ready) { DsmManager->SetDsmUpdateReady(ready); }
  int GetDsmUpdateReady() { return DsmManager->GetDsmUpdateReady(); }
  void ClearDsmUpdateReady() { return DsmManager->ClearDsmUpdateReady(); }

  // Description:
  // When sending, the writer can SetXMLDescriptionSend and it will be transmitted
  // to the receiver. When receiving, GetXMLDescriptionReceive queries the internal DSMBuffer
  // object to see if a string is present
  void SetXMLStringSend(const char *XMLStringSend) { DsmManager->SetXMLStringSend(XMLStringSend); }
  const char *GetXMLStringReceive() { return DsmManager->GetXMLStringReceive(); }
  void        ClearXMLStringReceive() { DsmManager->ClearXMLStringReceive(); }

  // Description:
  // Get the associated DSM buffer handle
  H5FDdsmBuffer *GetDSMHandle() { return DsmManager->GetDSMHandle(); }

  // Description:
  // Set/Get the current given steering command.
  // The command is either passed to the simulation or is printed into the GUI.
  // vtkSetStringMacro(SteeringCommand);
  void SetSteeringCommand(char *command);
  vtkGetStringMacro(SteeringCommand);

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
  bool   ReadDSMConfigFile() { return DsmManager->ReadDSMConfigFile(); }

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
    //ETX
    vtkMultiProcessController* Controller;
    //BTX
    //
    char           *XMFDescriptionFilePath;
    char           *SteeringCommand;
    //
    H5FDdsmManager *DsmManager;
#endif
    //ETX

private:
    vtkDSMManager(const vtkDSMManager&);  // Not implemented.
    void operator=(const vtkDSMManager&);  // Not implemented.
};

#endif
