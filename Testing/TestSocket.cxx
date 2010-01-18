/*=========================================================================

  Project                 : vtkCSCS
  Module                  : TestSocket.h
  Revision of last commit : $Rev: 1512 $
  Author of last commit   : $Author: soumagne $
  Date of last commit     : $Date:: 2010-01-13 14:45:20 +0100 #$

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
#include "TestSocket.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#include <winsock.h>
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

#include <cstring>

#if defined(_WIN32) && !defined(__CYGWIN__)
#define WSA_VERSION MAKEWORD(1,1)
#define XdmfCloseSocketMacro(sock) (closesocket(sock))
#else
#define XdmfCloseSocketMacro(sock) (close(sock))
#endif

#define QLEN 5

//-----------------------------------------------------------------------------
TestSocket::TestSocket()
{
  this->SocketDescriptor = -1;
  this->ClientSocketDescriptor = -1;
}

//-----------------------------------------------------------------------------
TestSocket::~TestSocket()
{
  if (this->ClientSocketDescriptor != -1) this->CloseClient();
  if (this->SocketDescriptor != -1) this->Close();
}

//-----------------------------------------------------------------------------
int TestSocket::WinSockInit()
{
#ifdef _WIN32
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;

  // Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h
  wVersionRequested = MAKEWORD(2, 2);
  err = WSAStartup(wVersionRequested, &wsaData);
  if (err != 0) {
    // Tell the user that we could not find a usable
    // Winsock DLL.
    printf("WSAStartup failed with error: %d\n", err);
    return -1;
  }

  // Confirm that the WinSock DLL supports 2.2.
  // Note that if the DLL supports versions greater
  // than 2.2 in addition to 2.2, it will still return
  // 2.2 in wVersion since that is the version we
  // requested.

  if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
    // Tell the user that we could not find a usable
    // WinSock DLL.
    printf("Could not find a usable version of Winsock.dll\n");
    WSACleanup();
    return -1;
  }
#endif
  return 0;
}

//-----------------------------------------------------------------------------
int TestSocket::WinSockCleanup()
{
  // Call WSACleanup when down using the Winsock dll
#ifdef _WIN32
  int err;
  err = WSACleanup();
  if (err != 0) {
    printf("WSACleanup failed with error: %d\n", err);
    return -1;
  }
#endif
  return 0;
}

//-----------------------------------------------------------------------------
int TestSocket::Create()
{
  if ((this->SocketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return -1;
  }
  // Eliminate windows 0.2 second delay sending (buffering) data.
  int on = 1;
  if (setsockopt(this->SocketDescriptor, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on))) {
    return -1;
  }
  return 0;
}
//-----------------------------------------------------------------------------
int TestSocket::Close()
{
  if (this->SocketDescriptor < 0) return -1;
  if (XdmfCloseSocketMacro(this->SocketDescriptor) < 0) return -1;
  this->SocketDescriptor = -1;
  return 0;
}

//-----------------------------------------------------------------------------
int TestSocket::CloseClient()
{
  if (this->ClientSocketDescriptor < 0) return -1;
  if (XdmfCloseSocketMacro(this->ClientSocketDescriptor) < 0) return -1;
  this->ClientSocketDescriptor = -1;
  return 0;
}

//-----------------------------------------------------------------------------
int TestSocket::Bind(int port, const char *hostName)
{
  struct sockaddr_in server;
  struct hostent *hp = NULL;

  server.sin_family = AF_INET;
  if (hostName != NULL) {
    hp = gethostbyname(hostName);
    if (!hp) {
      unsigned long addr = inet_addr(hostName);
      hp = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);
    }
  }
  if (!hp || (hostName == NULL)) {
    // XdmfErrorMacro("Unknown host: " << hostName);
    //server.sin_addr.s_addr = INADDR_ANY;
    char localhost[256];
    gethostname(localhost, sizeof(localhost));
    hp = gethostbyname(localhost);
    memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
  } else {
    memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
  }

  server.sin_port = htons(port);
  // Allow the socket to be bound to an address that is already in use
  int opt=1;
#ifdef _WIN32
  setsockopt(this->SocketDescriptor, SOL_SOCKET, SO_REUSEADDR, (char*) &opt, sizeof(int));
#else
  setsockopt(this->SocketDescriptor, SOL_SOCKET, SO_REUSEADDR, (void *) &opt, sizeof(int));
#endif

  if (bind(this->SocketDescriptor, reinterpret_cast<sockaddr*>(&server), sizeof(server))) {
    return -1;
  }
  return 0;
}

//-----------------------------------------------------------------------------
int TestSocket::Select(unsigned long msec)
{
  int socketdescriptor = -1;
  if (this->ClientSocketDescriptor < 0) {
    socketdescriptor = this->SocketDescriptor;
  } else {
    socketdescriptor = this->ClientSocketDescriptor;
  }

  if (socketdescriptor < 0) {
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
  FD_SET(socketdescriptor, &rset);
  int res = select(socketdescriptor + 1, &rset, 0, 0, tvalptr);
  if(res == 0) {
    return 0;//for time limit expire
  }

  if (res < 0 || !(FD_ISSET(socketdescriptor, &rset))) {
    // Some error.
    return -1;
  }
  // The indicated socket has some activity on it.
  return 1;
}

//-----------------------------------------------------------------------------
int TestSocket::Accept()
{
  struct sockaddr_in client;
#if !defined(_WIN32) || defined(__CYGWIN__)
  socklen_t client_len = sizeof(client);
#else
  int client_len = sizeof(client);
#endif

  if (this->SocketDescriptor < 0) {
    return -1;
  }
  this->ClientSocketDescriptor = accept(this->SocketDescriptor, reinterpret_cast<sockaddr*>(&client), &client_len);
  if (this->ClientSocketDescriptor < 0) {
    return -1;
  }
  return 0;
}

//-----------------------------------------------------------------------------
int TestSocket::Listen()
{
  if (this->SocketDescriptor < 0) {
    return -1;
  }
  return listen(this->SocketDescriptor, QLEN);
}

//-----------------------------------------------------------------------------
int TestSocket::Connect(const char* hostName, int port)
{
  if (this->SocketDescriptor < 0) {
    return -1;
  }

  struct hostent* hp;
  hp = gethostbyname(hostName);
  if (!hp) {
    // TODO replace inet_addr by inet_aton/inet_pton
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
}

//-----------------------------------------------------------------------------
int TestSocket::GetPort()
{
  struct sockaddr_in sockinfo;
  memset(&sockinfo, 0, sizeof(sockinfo));
#if !defined(_WIN32) || defined(__CYGWIN__)
  socklen_t sizebuf = sizeof(sockinfo);
#else
  int sizebuf = sizeof(sockinfo);
#endif
  if(getsockname(this->SocketDescriptor, reinterpret_cast<sockaddr*>(&sockinfo), &sizebuf) != 0) {
    return -1;
  }
  return ntohs(sockinfo.sin_port);
}

//-----------------------------------------------------------------------------
const char* TestSocket::GetHostName()
{
  struct sockaddr_in sockinfo;
  memset(&sockinfo, 0, sizeof(sockinfo));
#if !defined(_WIN32) || defined(__CYGWIN__)
  socklen_t sizebuf = sizeof(sockinfo);
#else
  int sizebuf = sizeof(sockinfo);
#endif
  if(getsockname(this->SocketDescriptor, reinterpret_cast<sockaddr*>(&sockinfo), &sizebuf) != 0) {
      return NULL;
  }
  return (const char*)inet_ntoa(sockinfo.sin_addr);
}

//-----------------------------------------------------------------------------
const char* TestSocket::GetLocalHostName()
{
  static char localhost[256];
  gethostname(localhost, sizeof(localhost));
  return localhost;
}

//-----------------------------------------------------------------------------
const char* TestSocket::GetLocalHostAddr()
{
  struct hostent *hp = NULL;
  struct sockaddr_in info;
  char localhost[256];
  gethostname(localhost, sizeof(localhost));
  hp = gethostbyname(localhost);
  memcpy(&info.sin_addr, hp->h_addr, hp->h_length);
  return (const char*)inet_ntoa(info.sin_addr);
}

//-----------------------------------------------------------------------------
int TestSocket::SelectSockets(const int *sockets_to_select, int size,
    unsigned long msec, int *selected_index)
{
  int i;
  int max_fd = -1;
  *selected_index = -1;
  if (size <  0) {
    return -1;
  }

  fd_set rset;
  struct timeval tval;
  struct timeval* tvalptr = 0;
  if (msec > 0) {
    tval.tv_sec = msec / 1000;
    tval.tv_usec = msec % 1000;
    tvalptr = &tval;
  }
  FD_ZERO(&rset);
  for (i=0; i<size; i++) {
    FD_SET(sockets_to_select[i],&rset);
    max_fd = (sockets_to_select[i] > max_fd)? sockets_to_select[i] : max_fd;
  }

  int res = select(max_fd + 1, &rset, 0, 0, tvalptr);
  if (res == 0) {
    return 0; //Timeout
  }
  if (res < 0) {
    // SelectSocket error.
    return -1;
  }

  //check which socket has some activity.
  for (i=0; i<size; i++) {
    if (FD_ISSET(sockets_to_select[i],&rset)) {
      *selected_index = i;
      return 1;
    }
  }
  return -1;
}

//-----------------------------------------------------------------------------
int TestSocket::Send(const void* data, int length)
{
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
    int flags, n;
#if defined(_WIN32) && !defined(__CYGWIN__)
    flags = 0;
#else
    // disabling, since not present on SUN.
    // flags = MSG_NOSIGNAL; //disable signal on Unix boxes.
    flags = 0;
#endif
    if (this->ClientSocketDescriptor < 0) {// Send from client to server
      n = send(this->SocketDescriptor, buffer+total, length-total, flags);
    } else {// Send from server to client
      n = send(this->ClientSocketDescriptor, buffer+total, length-total, flags);
    }
    if (n < 0) {
      return -1;
    }
    total += n;
  } while(total < length);
  return 0;
}

//-----------------------------------------------------------------------------
int TestSocket::Receive(void* data, int length, int readFully/*=1*/)
{
  if (!this->GetConnected()) {
    return -1;
  }

  char* buffer = reinterpret_cast<char*>(data);
  int total = 0;
  do {
    int n;
#if defined(_WIN32) && !defined(__CYGWIN__)
    int trys = 0;
#endif
    if (this->ClientSocketDescriptor < 0) {// Recv from Server to Client
      n = recv(this->SocketDescriptor, buffer+total, length-total, 0);
    } else {// Recv from Client to Server
      n = recv(this->ClientSocketDescriptor, buffer+total, length-total, 0);
    }
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
      return -1;
    }
    total += n;
  } while(readFully && total < length);
  return total;
}
