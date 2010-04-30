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

  char sendbuf1[] = "Bonjour le monde!";
  int sendLength1;
  char *recvbuf1 = NULL;
  int recvLength1;
  int port; char *hostName = NULL;
  char *big_chunk;
  int big_chunk_size = 1024*1024*1024;

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

  sock.WinSockInit();

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

  printf("Waiting now for connection on %s:%d...", sock.GetHostName(), sock.GetPort());
  fflush(stdout);
  if (sock.Accept() < 0) {
    fprintf(stderr, "Accept failed\n");
    return EXIT_FAILURE;
  }
  printf("ok\n");

  printf("%s(%s) is now connected\n", sock.GetLocalHostName(), sock.GetLocalHostAddr());

  printf("Receiving test message from client...");
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

  printf("Sending test message to client...");
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
  
  big_chunk = (char*) malloc(big_chunk_size);
  while(1) {
  int ret;
  
  printf("Receiving big chunk test from client...");
  fflush(stdout);
  ret = sock.Receive(big_chunk, big_chunk_size);
  if (ret < 0) {
	  fprintf(stderr, "Error in Socket Receive\n");
	  return EXIT_FAILURE;
  }
  printf("ok\n");
  }
  free(big_chunk);
  sock.WinSockCleanup();
  return EXIT_SUCCESS;
}
