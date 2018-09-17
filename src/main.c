#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>

char welcome[256] = "You have reached the Blackrock Server!!";

int serverSocket;
int clientSocket;

void error (const char *msg) {

    perror (msg);

    close (serverSocket);
    close (clientSocket);

    exit (1);

}

int main (void) {

    fprintf (stdout, "\n---> Blackrock Server <---\n\n");

    // create the server socket
    serverSocket = socket (AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) error ("Error creating server socket!\n");

    // define the server address
    struct sockaddr_in serverAddress;
    // clears any data in server address
    bzero ((char *) &serverAddress, sizeof (serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons (9002);  // arbitrary port for now
    serverAddress.sin_addr.s_addr = INADDR_ANY;     // ip = 0.0.0.0

    // bind the socket to our specify IP and port
    int binding = bind (serverSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress));
    if (binding < 0) error ("Error binding server socket!\n");

    // we can now listen for connections
    fprintf (stdout, "Waiting for connection..\n\n");
    struct sockaddr_in clientAddress;
    listen (serverSocket, 5);   // 5 is our number of connections
    socklen_t clientLen = sizeof (clientAddress);

    // accpet a connection
    clientSocket = accept (serverSocket, (struct sockaddr *) &clientAddress, &clientLen);
    if (clientSocket < 0) error ("Error accepting new connection!\n");
    else fprintf (stdout, "Client connected!\n");

    // send the welcome message if the connection is successfull
    send (clientSocket, welcome, sizeof (welcome), 0);

    // close the socket when we are done
    close (clientSocket);
    close (serverSocket);

    return 0;

}