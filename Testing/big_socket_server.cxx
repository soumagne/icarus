/*
 * socket_server.cxx
 *
 *  Created on: 13 jan. 2010
 *      Author: soumagne
 */
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "TestSocket.h"

int main(int argc, char *argv[]) {

  int port = 23000; char *hostName = NULL;
  int number_of_sockets = 0;
  TestSocket *testSocketArray[2048];

  if (argc == 2) {
    number_of_sockets = atoi(argv[1]);
  }
  else {
    fprintf(stderr, "Usage: %s number_of_sockets\n", argv[0]);
    return EXIT_FAILURE;
  }

  for (int i=0; i<number_of_sockets; i++) {
    testSocketArray[i] = new TestSocket();
    testSocketArray[i]->WinSockInit();

    if (testSocketArray[i]->Create() < 0) {
      fprintf(stderr, "Server cannot open socket\n");
      return EXIT_FAILURE;
    }

    while (1) {
      if (testSocketArray[i]->Bind(port, hostName) < 0) {
        port++;
        if (port == 100000) {
          fprintf(stderr, "Bind failed\n");
          return EXIT_FAILURE;
        }
      } else {
        break;
      }
    }

    if (testSocketArray[i]->Listen() < 0) {
      fprintf(stderr, "Listen failed\n");
      return EXIT_FAILURE;
    }
  }

  for (int i=0; i<number_of_sockets; i++) {
    testSocketArray[i]->WinSockCleanup();
    delete testSocketArray[i];
  }
  return EXIT_SUCCESS;
}
