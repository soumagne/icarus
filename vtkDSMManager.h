/*=========================================================================

  Project                 : vtkCSCS
  Module                  : vtkDSMManager.h
  Revision of last commit : $Rev$
  Author of last commit   : $Author$
  Date of last commit     : $Date::                            $

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

#include "XdmfDsm.h"         // Xdmf DSM objects
#include "XdmfDsmBuffer.h"   // Xdmf DSM objects
#include "XdmfDsmCommMpi.h"  // Xdmf DSM objects
#include "XdmfDOM.h"

#ifndef WIN32
  #define HAVE_PTHREADS
  #include <pthread.h>
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
  // Get the published name of our connection. 
  // Only valid after a PublishDSM call has been made.
  vtkSetStringMacro(PublishedPortName);
  vtkGetStringMacro(PublishedPortName);

  // Description:
  // Only valid after a AcceptConnection call has been made.
  vtkSetMacro(AcceptedConnection, bool);
  vtkGetMacro(AcceptedConnection, bool);

  vtkSetStringMacro(XMFDescriptionFilePath);
  vtkGetStringMacro(XMFDescriptionFilePath);

  bool   CreateDSM();
  bool   DestroyDSM();
  void   ClearDSM();
  void   ConnectDSM();
  void   DisconnectDSM();
  void   PublishDSM();
  void  *AcceptConnection();
  void   UnpublishDSM();
  void   H5Dump();
  void   H5DumpLight();
  void   H5DumpXML();
  void   GenerateXMFDescription();
  void   SendDSMXML();

//BTX
  XdmfDsmBuffer *GetDSMHandle();
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
    char          *FileName;
    int            NumberOfTimeSteps;
    int            TimeStep;
    int            UpdatePiece;
    int            UpdateNumPieces;
    vtkTypeInt64   LocalBufferSizeMBytes;

    //BTX
#ifdef HAVE_PTHREADS
    pthread_t      ServiceThread;
    pthread_t      ConnectionThread;
#elif HAVE_BOOST_THREADS
    boost::thread *ServiceThread;
    boost::thread *ConnectionThread;
#endif
    XdmfDsmBuffer *DSMBuffer;
    //ETX


    //BTX
#ifdef VTK_USE_MPI
    //ETX
    vtkMultiProcessController* Controller;
    //BTX
    XdmfDsmCommMpi *DSMComm;
    //
    char           *PublishedPortName;
    bool            AcceptedConnection;
    // bool            KillConnection;

    char           *XMFDescriptionFilePath;
    std::string     DumpDescription;
    std::string     GeneratedDescription;
#endif
    //ETX

private:
    vtkDSMManager(const vtkDSMManager&);  // Not implemented.
    void operator=(const vtkDSMManager&);  // Not implemented.
};

#endif
