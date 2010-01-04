#include "mpi.h"
#include <stdio.h>
#include <string.h>
#include <fstream>

#define MAX_DATA 50

int main( int argc, char *argv[] )
{
  int errs = 0;
  char port_name_out[MPI_MAX_PORT_NAME];
  char buf[MAX_DATA];
  int merr;
  char errmsg[MPI_MAX_ERROR_STRING];
  int msglen;
  int rank, size;
  MPI_Comm server;

  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if(size != 1) {
    printf("Too many processes, only one accepted\n");
    MPI_Finalize();
    return 1;
  }

  strcpy(buf, "Hello world!");

  MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

  strcpy(port_name_out, argv[1]);

  printf ("Our (client) port name is now %s \n", port_name_out);
  fflush(stdout);
  merr = MPI_Comm_connect(port_name_out, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &server);
  if (merr) {
    errs++;
    MPI_Error_string(merr, errmsg, &msglen);
    printf("Error in Lookup name: \"%s\"\n", errmsg);
  }

  MPI_Send(buf, MAX_DATA, MPI_CHAR, 0, 1, server);


  MPI_Comm_disconnect(&server);

  MPI_Finalize();

  printf("finalize ok\n");
  return 0;
}
