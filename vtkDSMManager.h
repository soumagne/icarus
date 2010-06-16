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

#include "H5FDdsm.h"         // Xdmf DSM objects
#include "H5FDdsmBuffer.h"   // Xdmf DSM objects
#include "H5FDdsmCommMpi.h"  // Xdmf DSM objects
#include "H5FDdsmIniFile.h"
#include "XdmfDOM.h"

#ifndef WIN32
  #define HAVE_PTHREADS
extern "C" {
  #include <pthread.h>
}
#elif HAVE_BOOST_THREADS
  #include <boost/thread/thread.hpp> // Boost Threads
#endif


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
  vtkSetMacro(LocalBufferSizeMBytes,int);
  vtkGetMacro(LocalBufferSizeMBytes,int);

  // Description:
  // Set/Get DsmIsServer info
  vtkSetMacro(DsmIsServer, int);
  vtkGetMacro(DsmIsServer, int);

  // Description:
  // Set/Get the interprocess communication subsystem
  vtkSetMacro(DsmCommType, int);
  vtkGetMacro(DsmCommType, int);

  // Description:
  // Set/Get the published host name of our connection.
  // Real value valid after a PublishDSM call has been made.
  vtkSetStringMacro(ServerHostName);
  vtkGetStringMacro(ServerHostName);

  // Description:
  // Set/Get the published port of our connection.
  // Real value valid after a PublishDSM call has been made.
  vtkSetMacro(ServerPort, int);
  vtkGetMacro(ServerPort, int);

  // Description:
  // Only valid after a AcceptConnection call has been made.
  int GetAcceptedConnection();

  // Description:
  // Get/Set the update ready flag which triggers the VTK pipeline update and the
  // display of DSM objects.
  vtkSetMacro(DsmUpdateReady, int);
  int GetDsmUpdateReady();
  void ClearDsmUpdateReady();

  // Description:
  // Set/Get the file path pointing either to an XDMF description file
  // or to the XDMF template used to generate a proper XDMF file.
  vtkSetStringMacro(XMFDescriptionFilePath);
  vtkGetStringMacro(XMFDescriptionFilePath);

  // Description:
  // When sending, the writer can SetXMLDescriptionSend and it will be transmitted
  // to the receiver. When receiving, GetXMLDescriptionReceive queries the internal DSMBuffer
  // object to see if a string is present
  vtkSetStringMacro(XMLStringSend);
  const char *GetXMLStringReceive();
  void        ClearXMLStringReceive();


  bool   CreateDSM();
  bool   DestroyDSM();
  void   ClearDSM();
  void   ConnectDSM();
  void   DisconnectDSM();
  void   PublishDSM();
  void   UnpublishDSM();
  void   H5Dump();
  void   H5DumpLight();
  void   H5DumpXML();
  void   GenerateXMFDescription();
  void   SendDSMXML();
  void   RequestRemoteChannel();

  // Description:
  // If the .dsm_config file exists in the standard location
  // $ENV{DSM_CONFIG_PATH}/.dsm_config or in the location set by
  // @CMAKE{H5FDdsm_CONFIG_PATH}/.dsm_config then the server/port/mode
  // information can be read. This is for use the by a DSM client.
  // DSM servers write their .dsm_config when PublishDSM() is called
  // Returns false if the .dsm_config file is not read
  bool   ReadDSMConfigFile();

//BTX
  H5FDdsmBuffer *GetDSMHandle();
//ETX

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
    vtkTypeInt64   LocalBufferSizeMBytes;

    //BTX
#ifdef HAVE_PTHREADS
    pthread_t      ServiceThread;
#elif HAVE_BOOST_THREADS
    boost::thread *ServiceThread;
#endif
    //ETX

    //BTX
#ifdef VTK_USE_MPI
    //ETX
    vtkMultiProcessController* Controller;
    //BTX
    H5FDdsmBuffer  *DSMBuffer;
    H5FDdsmComm    *DSMComm;
    //
    int             DsmIsServer;
    int             DsmCommType;
    char           *ServerHostName;
    int             ServerPort;
    //
    int             DsmUpdateReady;
    //
    char           *XMFDescriptionFilePath;
    char           *XMLStringSend;
#endif
    //ETX

private:
    vtkDSMManager(const vtkDSMManager&);  // Not implemented.
    void operator=(const vtkDSMManager&);  // Not implemented.
};

#endif
