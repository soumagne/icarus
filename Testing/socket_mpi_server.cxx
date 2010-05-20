/*
 * socket_mpi_server.cxx
 *
 *  Created on: 10 déc. 2009
 *      Author: soumagne
 */
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mpi.h>

#ifndef TESTSOCKET
#include "H5FDdsmSocket.h"
#else
#include "TestSocket.h"
#endif

int main(int argc, char *argv[]) {

  char *recvbuf1, *recvbuf2;
  int recvLength1 ,recvLength2;
  int err=0, rank, nprocs;
  MPI_Comm intercomm;
  MPI_Status status;
  int port; char *hostName = NULL;
#ifndef TESTSOCKET
  H5FDdsmSocket sock;
#else
  TestSocket sock;
#endif

  MPI_Init( &argc, &argv );
  MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (nprocs != 1) {
    fprintf(stderr, "Only one process accepted\n");
    MPI_Finalize();
    return 0;
  }

  if (argc == 2) {
    port = atoi(argv[1]);
  }
  else if (argc == 3) {
    hostName = argv[1];
    port = atoi(argv[2]);
  }
  else {
    fprintf(stderr, "Usage: %s [hostName] port\n", argv[0]);
    MPI_Finalize();
    return 0;
  }

  if (sock.Create() < 0) {
    fprintf(stderr, "Server cannot open socket\n");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  if (sock.Bind(port, hostName) < 0) {
    fprintf(stderr, "Bind failed\n");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  if (sock.Listen() < 0) {
    fprintf(stderr, "Listen failed\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  printf("Waiting now for connection on port %d...", port);
  if (sock.Accept() < 0) {
    fprintf(stderr, "Accept failed\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  printf("ok\n");

  printf("Receiving test message using sockets only...");
  if (sock.Receive(&recvLength1, sizeof(recvLength1)) < 0) {
      fprintf(stderr, "Error in Socket Receive\n");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
  recvbuf1 = (char*) malloc(recvLength1);
  if (sock.Receive(recvbuf1, recvLength1) < 0) {
    fprintf(stderr, "Error in Socket Receive\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  printf("ok\nServer received: %s\n", recvbuf1);
  free(recvbuf1);

  MPI_Barrier(MPI_COMM_WORLD);

  printf("Joining now communicators...");
  err = MPI_Comm_join(sock.GetClientSocketDescriptor(), &intercomm);
  if (err) {
    fprintf(stderr, "Error in MPI_Comm_join %d\n", err);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  printf("ok\n");

  printf("Receiving test message using new MPI communicator...");
  MPI_Probe(0, 0, intercomm, &status);
  MPI_Get_count(&status, MPI_CHAR, &recvLength2);
  recvbuf2 = (char*) malloc(recvLength2);
  err = MPI_Recv(recvbuf2, recvLength2, MPI_CHAR, 0, 0, intercomm, MPI_STATUS_IGNORE);
  if (err != MPI_SUCCESS) {
    fprintf(stderr, "Error in MPI_Recv on new communicator\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  printf("ok\nServer received: %s\n", recvbuf2);
  free(recvbuf2);

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
