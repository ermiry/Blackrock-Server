#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <netinet/in.h>

char welcome[256] = "You have reached the Blackrock Server!!";

int main (void) {

    fprintf (stdout, "\n---> Blackrock Server <---\n\n");

    // create the server socket
    int serverSocket = socket (AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        fprintf (stderr, "Error creating server socket!\n");
        return 1;
    }

    // define the server address
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons (9002);  // arbitrary port for now
    serverAddress.sin_addr.s_addr = INADDR_ANY;     // ip = 0.0.0.0

    // bind the socket to our specify IP and port
    int binding = bind (serverSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress));
    if (binding < 0) {
        fprintf (stderr, "Error binding server socket!\n");
        return 1;
    }

    // we can now listen for connections
    listen (serverSocket, 5);   // 5 is our number of connections

    // accpet a connection
    int clientSocket = accept (serverSocket, NULL, NULL);

    // send the welcome message if the connection is successfull
    send (clientSocket, welcome, sizeof (welcome), 0);

    // close the socket when we are done
    close (serverSocket);

    return 0;

}