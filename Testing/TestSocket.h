/*=========================================================================

  Project                 : vtkCSCS
  Module                  : TestSocket.h
  Revision of last commit : $Rev: 1481 $
  Author of last commit   : $Author: soumagne $
  Date of last commit     : $Date:: 2009-12-11 16:30:26 +0100 #$

  Copyright (C) CSCS - Swiss National Supercomputing Centre.
  You may use modify and and distribute this code freely providing
  1) This copyright notice appears on all copies of source code
  2) An acknowledgment appears with any substantial usage of the code
  3) If this code is contributed to any other open source project, it
  must not be reformatted such that the indentation, bracketing or
  overall style is modified significantly.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
=========================================================================

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSocket - BSD socket encapsulation.
// .SECTION Description
// This abstract class encapsulates a BSD socket. It provides an API for
// basic socket operations.

#ifndef TESTSOCKET_H
#define TESTSOCKET_H

#include <cstdlib>

#define GetValueMacro(var,type) \
type Get##var () \
  { \
  return ( this->var ); \
  }

class TestSocket {

public:
  TestSocket();
  ~TestSocket();

  // ----- Status API ----
  // Description:
  // Check if the socket is alive.
  int GetConnected() { return (this->SocketDescriptor >=0); }

  GetValueMacro(SocketDescriptor, int);
  GetValueMacro(ClientSocketDescriptor, int);

  // Description:
  // Creates an endpoint for communication and returns the descriptor.
  // -1 indicates error.
  int Create();

  // Description:
  // Close the socket.
  int Close();

  // Description:
  // Close the client socket if it exists.
  int CloseClient();

  // Description:
  // Binds socket to a particular port.
  // Returns 0 on success other -1 is returned.
  int Bind(int port, const char *hostName=NULL);

  // Description:
  // Selects a socket ie. waits for it to change status.
  // Returns 1 on success; 0 on timeout; -1 on error. msec=0 implies
  // no timeout.
  int Select(unsigned long msec);

  // Description:
  // Accept a connection on a socket. Returns -1 on error. Otherwise
  // the descriptor of the accepted socket.
  int Accept();

  // Description:
  // Listen for connections on a socket. Returns 0 on success. -1 on error.
  int Listen();

  // Description:
  // Connect to a server socket. Returns 0 on success, -1 on error.
  int Connect(const char* hostname, int port);

  // Description:
  // Returns the port to which the socket is connected.
  // -1 on error.
  int GetPort();

  // ------ Communication API --- // Should never be used
  // Description:
  // These methods send data over the socket.
  // Returns 0 on success, -1 on error.
  int Send(const void* data, int length);

  // Description:
  // Receive data from the socket.
  // This call blocks until some data is read from the socket.
  // When readFully is set, this call will block until all the
  // requested data is read from the socket.
  // -1 on error, else number of bytes read is returned.
  int Receive(void* data, int length, int readFully=1);

protected:
  int SocketDescriptor;
  int ClientSocketDescriptor;
};


#endif /* TESTSOCKET_H */
