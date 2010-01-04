#include "mpi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>

#define MAX_DATA 50

int main( int argc, char *argv[] )
{
  int errs = 0;
  char port_name[MPI_MAX_PORT_NAME];
  int merr;
  char errmsg[MPI_MAX_ERROR_STRING];
  int msglen;
  int rank, size;
  char buf[MAX_DATA];
  MPI_Status status;
  MPI_Comm client;

  MPI_Init( &argc, &argv );

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if(size != 1) {
    printf("Too many processes, only one accepted\n");
    MPI_Finalize();
    return 1;
  }

  MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

  MPI_Open_port(MPI_INFO_NULL, port_name);
  printf ("Our port name is %s \n", port_name);

  merr = MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &client);
  if (merr) {
    errs++;
    MPI_Error_string(merr, errmsg, &msglen);
    printf("Error in MPI_Comm_accept : \"%s\"\n", errmsg);
  }

  MPI_Recv(buf, MAX_DATA, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, client, &status);

  printf("Server received: %s\n", buf);

  MPI_Close_port(port_name);
  MPI_Finalize();
  printf("finalize ok\n");
  return EXIT_SUCCESS;
}
