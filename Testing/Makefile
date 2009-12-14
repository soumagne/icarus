CXX = mpicxx
LIBS = -lmpich

all: socket_mpi_client socket_mpi_server

socket_mpi_client:	socket_mpi_client.cxx TestSocket.cxx TestSocket.h
	${CXX} -DTESTSOCKET socket_mpi_client.cxx TestSocket.cxx -o $@ ${LIBS}

socket_mpi_server:      socket_mpi_server.cxx TestSocket.cxx TestSocket.h
	${CXX} -DTESTSOCKET socket_mpi_server.cxx TestSocket.cxx -o $@ ${LIBS}

clean:
	rm socket_mpi_client socket_mpi_server
