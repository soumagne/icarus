/*
 * socket_mpi_client.cxx
 *
 *  Created on: 10 déc. 2009
 *      Author: soumagne
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mpi.h>

#include "XdmfDsmSocket.h"

int main(int argc, char *argv[]) {

  char sendbuf1[] = "Hello World!";
  int sendLength1;
  char sendbuf2[] = "My name is Bob";
  int err=0, rank, nprocs;
  MPI_Comm intercomm;
  int port;
  char hostname[MPI_MAX_PROCESSOR_NAME];
  XdmfDsmSocket dsmSock;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (nprocs != 1) {
    fprintf(stderr, "Only one process accepted\n");
    MPI_Finalize();
    return 0;
  }

  if (argc != 3) {
    fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
    MPI_Finalize();
    return 0;
  }

  strcpy(hostname, argv[1]);
  port = atoi(argv[2]);

  if(dsmSock.Create() < 0) {
    fprintf(stderr, "Unable to create socket\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  printf("Attempting to connect to %s:%d...", hostname, port);
  if(dsmSock.Connect(hostname, port) < 0) {
    fprintf(stderr, "Unable to connect to %s:%d\n", hostname, port);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  printf("ok\n");

  printf("Sending test message using sockets only...");
  sendLength1 = strlen(sendbuf1)+1;
  if (dsmSock.Send(&sendLength1, sizeof(int)) < 0) {
    fprintf(stderr, "Error in Socket Send\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  if (dsmSock.Send(sendbuf1, sendLength1) < 0) {
    fprintf(stderr, "Error in Socket Send\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  printf("ok\n");

  MPI_Barrier(MPI_COMM_WORLD);

  printf("Joining now communicators...");
  err = MPI_Comm_join(dsmSock.GetSocketDescriptor(), &intercomm);
  if (err) {
    fprintf(stderr, "Error in MPI_Comm_join %d\n", err);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  printf("ok\n");

  printf("Sending test message using new MPI communicator...");
  err = MPI_Send(sendbuf2, strlen(sendbuf2)+1, MPI_CHAR, 0, 0, intercomm);
  if (err != MPI_SUCCESS) {
    fprintf(stderr, "Error in MPI_Send on new communicator\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  printf("ok\n");

  printf("Disconnecting...");
  err = MPI_Comm_disconnect(&intercomm);
  if (err != MPI_SUCCESS) {
    fprintf(stderr, "Error in MPI_Comm_disconnect\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  printf("ok\n");

  MPI_Finalize();
  return err;
}
