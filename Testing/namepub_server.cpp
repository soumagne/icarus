#include "mpi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>

#define MAX_DATA 50

//char test[] = "tag=0 port=1957 description=agno ifname=148.187.130.32";

int main( int argc, char *argv[] )
{
    int errs = 0;
    char port_name[MPI_MAX_PORT_NAME];
    char serv_name[256];
    int merr;
    char errmsg[MPI_MAX_ERROR_STRING];
    int msglen;
    int rank, size;
    char buf[MAX_DATA];
    MPI_Status status;
    MPI_Comm client;
    MPI_Info info;

    MPI_Init( &argc, &argv );

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if(size != 1) {
      printf("Too many processes, only one accepted\n");
      MPI_Finalize();
      return 1;
    }

#ifdef WIN_32
    std::ofstream fserver("//ponte.cscs.ch/home/biddisco/DSM/agno_name.txt");
#else
    std::ofstream fserver("/home/biddisco/DSM/agno_name.txt");
#endif

    //strcpy(serv_name, "MyTest");

    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

    //MPI_Info_create (&info);
    //MPI_Info_set( info, "ip_port", "1957");
    //MPI_Info_set( info, "ip_address", "148.187.130.32");
      
    //strcpy(port_name, argv[1]);
    MPI_Open_port(MPI_INFO_NULL, port_name);
    printf ("Our port name is %s \n", port_name);      
    fserver << port_name << std::endl;
    fserver.close();

    fflush(stdout);

//    merr = MPI_Publish_name(serv_name, MPI_INFO_NULL, port_name);
//    merr = 0;
//    if (merr) {
//      errs++;
 //     MPI_Error_string(merr, errmsg, &msglen);
 //     printf("Error in Publish_name: \"%s\"\n", errmsg);
 //   }
 //   else {
 //     printf("Published port_name( %s )\n", port_name);
 //   }

    merr = MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &client);
    if (merr) {
      errs++;
      MPI_Error_string(merr, errmsg, &msglen);
      printf("Error in MPI_Comm_accept : \"%s\"\n", errmsg);
    } 

//    MPI_Recv(buf, MAX_DATA, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, client, &status);

//    printf("Server received: %s\n", buf);

 //   merr = MPI_Unpublish_name(serv_name, MPI_INFO_NULL, port_name);
 //   if (merr) {
 //     errs++;
 //     MPI_Error_string(merr, errmsg, &msglen);
 //     printf("Error in Unpublish name: \"%s\"\n", errmsg);
 //   }

    MPI_Close_port(port_name);
    MPI_Finalize();
    printf("finalize ok\n");
    return EXIT_SUCCESS;
}

