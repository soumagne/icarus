/*=========================================================================

  Project                 : vtkCSCS
  Module                  : XdmfDsmSocket.h
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
=========================================================================

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "XdmfDsmSocket.h"

// TODO The XDMF_SOCKET_FAKE_API definition is given to the compiler
// command line by CMakeLists.txt if there is no real sockets
// interface available.  When this macro is defined we simply make
// every method return failure.
//
// Perhaps we should add a method to query at runtime whether a real
// sockets interface is available.

#ifndef XDMF_SOCKET_FAKE_API // Really needed ??
#if defined(_WIN32) && !defined(__CYGWIN__)
#define VTK_WINDOWS_FULL
#include "vtkWindows.h"
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#endif
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#define WSA_VERSION MAKEWORD(1,1)
#define XdmfCloseSocketMacro(sock) (closesocket(sock))
#else
#define XdmfCloseSocketMacro(sock) (close(sock))
#endif

#if defined(__BORLANDC__)
# pragma warn -8012 /* signed/unsigned comparison */
#endif

#if !defined(_WIN32)
#define XDMF_HAVE_GETSOCKNAME_WITH_SOCKLEN_T
//#define XDMF_HAVE_SO_REUSEADDR
#endif
//-----------------------------------------------------------------------------
XdmfDsmSocket::XdmfDsmSocket()
{
  this->SocketDescriptor = -1;
}

//-----------------------------------------------------------------------------
XdmfDsmSocket::~XdmfDsmSocket()
{
  if (this->SocketDescriptor != -1) {
    this->Close();
    this->SocketDescriptor = -1;
  }
}

//-----------------------------------------------------------------------------
int XdmfDsmSocket::Create()
{
#ifndef XDMF_SOCKET_FAKE_API
  this->SocketDescriptor = socket(AF_INET, SOCK_STREAM, 0);
  // Eliminate windows 0.2 second delay sending (buffering) data.
  int on = 1;
  if (setsockopt(this->SocketDescriptor, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on))) {
    return -1;
  }
  return 0;
#else
  return -1;
#endif
}

//-----------------------------------------------------------------------------
int XdmfDsmSocket::Bind(int port)
{
#ifndef XDMF_SOCKET_FAKE_API
  struct sockaddr_in server;

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(port);
  // Allow the socket to be bound to an address that is already in use
  int opt=1;
#ifdef _WIN32
  setsockopt(this->SocketDescriptor, SOL_SOCKET, SO_REUSEADDR, (char*) &opt, sizeof(int));
#elif defined(XDMF_HAVE_SO_REUSEADDR)
  setsockopt(this->SocketDescriptor, SOL_SOCKET, SO_REUSEADDR, (void *) &opt, sizeof(int));
#endif

  if (bind(this->SocketDescriptor, reinterpret_cast<sockaddr*>(&server), sizeof(server))) {
    return -1;
  }
  return 0;
#else
  static_cast<void>(port);
  return -1;
#endif
}

//-----------------------------------------------------------------------------
int XdmfDsmSocket::Accept() {
#ifndef XDMF_SOCKET_FAKE_API
  if (this->SocketDescriptor < 0) {
    return -1;
  }
  return accept(this->SocketDescriptor, 0, 0);
#else
  return -1;
#endif
}

//-----------------------------------------------------------------------------
int XdmfDsmSocket::Listen()
{
#ifndef XDMF_SOCKET_FAKE_API
  if (this->SocketDescriptor < 0) {
    return -1;
  }
  return listen(this->SocketDescriptor, 1);
#else
  return -1;
#endif
}

//-----------------------------------------------------------------------------
int XdmfDsmSocket::Select(unsigned long msec)
{
#ifndef XDMF_SOCKET_FAKE_API
  if (this->SocketDescriptor < 0 ) {
    // invalid socket descriptor.
    return -1;
  }

  fd_set rset;
  struct timeval tval;
  struct timeval* tvalptr = 0;
  if (msec > 0) {
    tval.tv_sec = msec / 1000;
    tval.tv_usec = (msec % 1000)*1000;
    tvalptr = &tval;
  }
  FD_ZERO(&rset);
  FD_SET(this->SocketDescriptor, &rset);
  int res = select(this->SocketDescriptor + 1, &rset, 0, 0, tvalptr);
  if(res == 0) {
    return 0;//for time limit expire
  }

  if (res < 0 || !(FD_ISSET(this->SocketDescriptor, &rset))) {
    // Some error.
    return -1;
  }
  // The indicated socket has some activity on it.
  return 1;
#else
  static_cast<void>(msec);
  return -1;
#endif
}

//-----------------------------------------------------------------------------
int XdmfDsmSocket::Connect(const char* hostName, int port)
{
#ifndef XDMF_SOCKET_FAKE_API
  if (this->SocketDescriptor < 0) {
    return -1;
  }

  struct hostent* hp;
  hp = gethostbyname(hostName);
  if (!hp) {
    unsigned long addr = inet_addr(hostName);
    hp = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);
  }

  if (!hp) {
    // XdmfErrorMacro("Unknown host: " << hostName);
    return -1;
  }

  struct sockaddr_in name;
  name.sin_family = AF_INET;
  memcpy(&name.sin_addr, hp->h_addr, hp->h_length);
  name.sin_port = htons(port);

  return connect(this->SocketDescriptor, reinterpret_cast<sockaddr*>(&name),
      sizeof(name));
#else
  static_cast<void>(hostName);
  static_cast<void>(port);
  return -1;
#endif
}

//-----------------------------------------------------------------------------
int XdmfDsmSocket::GetPort()
{
#ifndef XDMF_SOCKET_FAKE_API
  struct sockaddr_in sockinfo;
  memset(&sockinfo, 0, sizeof(sockinfo));
#if defined(XDMF_HAVE_GETSOCKNAME_WITH_SOCKLEN_T)
  socklen_t sizebuf = sizeof(sockinfo);
#else
  int sizebuf = sizeof(sockinfo);
#endif
  if(getsockname(this->SocketDescriptor, reinterpret_cast<sockaddr*>(&sockinfo), &sizebuf) != 0) {
    return -1;
  }
  return ntohs(sockinfo.sin_port);
#else
  return -1;
#endif
}

//-----------------------------------------------------------------------------
int XdmfDsmSocket::Close()
{
#ifndef XDMF_SOCKET_FAKE_API
  if (this->SocketDescriptor < 0) {
    return -1;
  }
  return XdmfCloseSocketMacro(this->SocketDescriptor);
#else
  return -1;
#endif
}

//-----------------------------------------------------------------------------
int XdmfDsmSocket::Send(const void* data, int length)
{
#ifndef XDMF_SOCKET_FAKE_API
  if (!this->GetConnected()) {
    return -1;
  }
  if (length == 0) {
    // nothing to send.
    return 0;
  }
  const char* buffer = reinterpret_cast<const char*>(data);
  int total = 0;
  do {
    int flags;
#if defined(_WIN32) && !defined(__CYGWIN__)
    flags = 0;
#else
    // disabling, since not present on SUN.
    // flags = MSG_NOSIGNAL; //disable signal on Unix boxes.
    flags = 0;
#endif
    int n = send(this->SocketDescriptor, buffer+total, length-total, flags);
    if(n < 0) {
      XdmfErrorMessage("Socket Error: Send failed.");
      return -1;
    }
    total += n;
  } while(total < length);
  return 0;
#else
  static_cast<void>(data);
  static_cast<void>(length);
  return -1;
#endif
}

//-----------------------------------------------------------------------------
int XdmfDsmSocket::Receive(void* data, int length, int readFully/*=1*/)
{
#ifndef XDMF_SOCKET_FAKE_API
  if (!this->GetConnected()) {
    return -1;
  }

  char* buffer = reinterpret_cast<char*>(data);
  int total = 0;
  do {
#if defined(_WIN32) && !defined(__CYGWIN__)
    int trys = 0;
#endif
    int n = recv(this->SocketDescriptor, buffer+total, length-total, 0);
    if(n < 1) {
#if defined(_WIN32) && !defined(__CYGWIN__)
      // On long messages, Windows recv sometimes fails with WSAENOBUFS, but
      // will work if you try again.
      int error = WSAGetLastError();
      if ((error == WSAENOBUFS) && (trys++ < 1000)) {
        Sleep(1);
        continue;
      }
#else
      // On unix, a recv may be interrupted by a signal.  In this case we should
      // retry.
      int errorNumber = errno;
      if (errorNumber == EINTR) continue;
#endif
      XdmfErrorMessage("Socket Error: Receive failed.");
      return -1;
    }
    total += n;
  } while(readFully && total < length);
  return total;
#else
  static_cast<void>(data);
  static_cast<void>(length);
  static_cast<void>(readFully);
  return -1;
#endif
}
