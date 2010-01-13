/*
 * socket_server.cxx
 *
 *  Created on: 13 jan. 2010
 *      Author: soumagne
 */
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifndef TESTSOCKET
  #include "XdmfDsmSocket.h"
#else
  #include "TestSocket.h"
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
  #include <windows.h>
  #include <winsock.h>
#endif
int main(int argc, char *argv[]) {

  char *recvbuf1;
  int recvLength1;
  int port; char *hostName = NULL;

#ifdef _WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        printf("WSAStartup failed with error: %d\n", err);
        return 1;
    }

    /* Confirm that the WinSock DLL supports 2.2.*/
    /* Note that if the DLL supports versions greater    */
    /* than 2.2 in addition to 2.2, it will still return */
    /* 2.2 in wVersion since that is the version we      */
    /* requested.                                        */

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        printf("Could not find a usable version of Winsock.dll\n");
        WSACleanup();
        return 1;
    }

#endif

#ifndef TESTSOCKET
  XdmfDsmSocket *sock = new XdmfDsmSocket();
#else
  TestSocket sock;
#endif

  if (argc == 2) {
    port = atoi(argv[1]);
  }
  else if (argc == 3) {
    hostName = argv[1];
    port = atoi(argv[2]);
  }
  else {
    fprintf(stderr, "Usage: %s [hostName] port\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (sock->Create() < 0) {
    fprintf(stderr, "Server cannot open socket\n");
    return EXIT_FAILURE;
  }

  if (sock->Bind(port, hostName) < 0) {
    fprintf(stderr, "Bind failed\n");
    return EXIT_FAILURE;
  }

  if (sock->Listen() < 0) {
    fprintf(stderr, "Listen failed\n");
    return EXIT_FAILURE;
  }

  printf("Waiting now for connection on port %d...", port);
  fflush(stdout);
  if (sock->Accept() < 0) {
    fprintf(stderr, "Accept failed\n");
    return EXIT_FAILURE;
  }
  printf("ok\n");

  printf("Receiving test message using sockets only...");
  fflush(stdout);
  if (sock->Receive(&recvLength1, sizeof(recvLength1)) < 0) {
      fprintf(stderr, "Error in Socket Receive\n");
      return EXIT_FAILURE;
    }
  recvbuf1 = (char*) malloc(recvLength1);
  if (sock->Receive(recvbuf1, recvLength1) < 0) {
    fprintf(stderr, "Error in Socket Receive\n");
    return EXIT_FAILURE;
  }
  printf("ok\nServer received: %s\n", recvbuf1);
  free(recvbuf1);


/* then call WSACleanup when down using the Winsock dll */
    
#ifdef _WIN32
    WSACleanup();
#endif

  return EXIT_SUCCESS;
}
