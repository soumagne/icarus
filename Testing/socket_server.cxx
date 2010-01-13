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

int main(int argc, char *argv[]) {

  char *recvbuf1;
  int recvLength1;
  int port; char *hostName = NULL;
#ifndef TESTSOCKET
  XdmfDsmSocket sock;
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

  if (sock.Create() < 0) {
    fprintf(stderr, "Server cannot open socket\n");
    return EXIT_FAILURE;
  }

  if (sock.Bind(port, hostName) < 0) {
    fprintf(stderr, "Bind failed\n");
    return EXIT_FAILURE;
  }

  if (sock.Listen() < 0) {
    fprintf(stderr, "Listen failed\n");
    return EXIT_FAILURE;
  }

  printf("Waiting now for connection on port %d...", port);
  fflush(stdout);
  if (sock.Accept() < 0) {
    fprintf(stderr, "Accept failed\n");
    return EXIT_FAILURE;
  }
  printf("ok\n");

  printf("Receiving test message using sockets only...");
  fflush(stdout);
  if (sock.Receive(&recvLength1, sizeof(recvLength1)) < 0) {
      fprintf(stderr, "Error in Socket Receive\n");
      return EXIT_FAILURE;
    }
  recvbuf1 = (char*) malloc(recvLength1);
  if (sock.Receive(recvbuf1, recvLength1) < 0) {
    fprintf(stderr, "Error in Socket Receive\n");
    return EXIT_FAILURE;
  }
  printf("ok\nServer received: %s\n", recvbuf1);
  free(recvbuf1);

  return EXIT_SUCCESS;
}
