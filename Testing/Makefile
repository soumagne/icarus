CXX = g++
LIBS = 

all: socket_client socket_server

socket_client:	socket_client.cxx TestSocket.cxx TestSocket.h
	${CXX} -Wall -DTESTSOCKET socket_client.cxx TestSocket.cxx -o $@ ${LIBS}

socket_server:      socket_server.cxx TestSocket.cxx TestSocket.h
	${CXX} -Wall -DTESTSOCKET socket_server.cxx TestSocket.cxx -o $@ ${LIBS}

clean:
	rm *.o socket_client.p.* socket_server.p.*

distclean:
	rm *.o socket_client.p.* socket_server.p.* socket_client socket_server

