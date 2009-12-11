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

#include "XdmfDsmSocket.h"

int main(int argc, char *argv[]) {

  int recvbuf[15];
  int err=0, rank, nprocs;
  MPI_Comm intercomm;
  int port;
  XdmfDsmSocket dsmSock;

  MPI_Init( &argc, &argv );
  MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (nprocs != 1) {
    fprintf(stderr, "Only one process accepted\n");
    MPI_Finalize();
    return 0;
  }

  if (argc != 2) {
     fprintf(stderr, "Usage: %s port\n", argv[0]);
     MPI_Finalize();
     return 0;
   }

  port = atoi(argv[1]);

  if (dsmSock.Create() < 0) {
    fprintf(stderr, "Server cannot open socket\n");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  if (dsmSock.Bind(port) < 0) {
    fprintf(stderr, "Bind failed\n");
    MPI_Abort(MPI_COMM_WORLD,1);
  }

  if (dsmSock.Listen() < 0) {
    fprintf(stderr, "Listen failed\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  if (dsmSock.Accept() < 0) {
    fprintf(stderr, "Accept failed\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  MPI_Barrier(MPI_COMM_WORLD);
//  MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
//  err = MPI_Comm_join(dsmSock.GetSocketDescriptor(), &intercomm);
//  if (err) {
//    fprintf(stderr, "Error in MPI_Comm_join %d\n", err);
//  }
//  MPI_Comm_set_errhandler(intercomm, MPI_ERRORS_RETURN);

//  err = MPI_Recv(recvbuf, 15, MPI_CHAR, 0, 0, intercomm, MPI_STATUS_IGNORE);
//  if (err != MPI_SUCCESS) {
//    fprintf(stderr, "Error in MPI_Recv on new communicator\n");
//  }
dsmSock.Receive(recvbuf, 5);
  printf("Server received: %s\n", recvbuf);


//  err = MPI_Comm_disconnect(&intercomm);
//  if (err != MPI_SUCCESS) {
//    fprintf(stderr, "Error in MPI_Comm_disconnect\n");
//  }
  MPI_Finalize();
  return err;
}
