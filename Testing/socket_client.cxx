/*
 * socket_client.cxx
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

  char sendbuf1[] = "Hello World!";
  int sendLength1;
  char *recvbuf1 = NULL;
  int recvLength1;
  int port;
  char hostname[256];
#ifndef TESTSOCKET
  XdmfDsmSocket sock;
#else
  TestSocket sock;
#endif

  if (argc != 3) {
    fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
    return EXIT_FAILURE;
  }

  strcpy(hostname, argv[1]);
  port = atoi(argv[2]);

  sock.WinSockInit();

  if(sock.Create() < 0) {
    fprintf(stderr, "Unable to create socket\n");
    return EXIT_FAILURE;
  }

  printf("Attempting to connect to %s:%d...", hostname, port);
  fflush(stdout);
  if(sock.Connect(hostname, port) < 0) {
    fprintf(stderr, "Unable to connect to %s:%d\n", hostname, port);
    return EXIT_FAILURE;
  }
  printf("ok\n");

  printf("%s(%s) is now connected\n", sock.GetLocalHostName(), sock.GetLocalHostAddr());

  printf("Sending test message to server...");
  fflush(stdout);
  sendLength1 = strlen(sendbuf1)+1;
  if (sock.Send(&sendLength1, sizeof(int)) < 0) {
    fprintf(stderr, "Error in Socket Send\n");
    return EXIT_FAILURE;
  }
  if (sock.Send(sendbuf1, sendLength1) < 0) {
    fprintf(stderr, "Error in Socket Send\n");
    return EXIT_FAILURE;
  }
  printf("ok\n");

  printf("Receiving test message from server...");
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
  printf("ok\nClient received: %s\n", recvbuf1);
  free(recvbuf1);
  sock.WinSockCleanup();

  return EXIT_SUCCESS;
}
