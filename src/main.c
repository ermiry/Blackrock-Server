#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>

#include <pthread.h>

#define PORT    9001

// FIXME:
// #define SERVER_ADDRESS  "192.168.1.7"

char welcome[256] = "You have reached the Blackrock Server!!";
const char filepath[64] = "./data/test.txt";

int serverSocket;
int clientSocket;

void error (const char *msg) {

    perror (msg);

    close (serverSocket);
    close (clientSocket);

    exit (1);

}

int initServer (struct sockaddr_in serverAddress) {

    // create the server socket
    int ssocket = socket (AF_INET, SOCK_STREAM, 0);
    if (ssocket < 0) error ("Error creating server socket!\n");

    // define the server address
    memset (&serverAddress, 0, sizeof (serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    // inet_pton(AF_INET, SERVER_ADDRESS, &(server_addr.sin_addr));
    serverAddress.sin_port = htons (PORT);

    // bind the socket to our specify IP and port
    if (bind (ssocket, (struct sockaddr *) &serverAddress, sizeof (struct sockaddr)) < 0)
        error ("Error binding server socket!\n");

    return ssocket;

}

int fileData (const char *filepath, struct stat fileStats, char fileSize[]) {

    // prepare the file to send
    int fd = open (filepath, O_RDONLY);
    if (fd < 0) error ("Error openning file!\n");

    // get file stats
    if (fstat (fd, &fileStats) < 0) error ("Error getting file stats!\n");

    sprintf (fileSize, "%ld", fileStats.st_size);

    fprintf (stdout, "File size: %ld bytes.\n", fileStats.st_size);

    return fd;

}

int sendFile (int peerSocket, struct stat fileStats, int fd, char *fileSize) {

    // Seinding file size
    ssize_t len;

    len = send (peerSocket, fileSize, sizeof (fileSize), 0);
    if (len < 0) error ("Error sending file size!\n");
    else fprintf (stdout, "Server sent %ld bytes for the size.\n", len);

    // sending file data
    off_t offset = 0;
    int remainData = fileStats.st_size;
    int sentBytes = 0;

    while (((sentBytes = sendfile (peerSocket, fd, &offset, BUFSIZ)) > 0) && (remainData > 0)) {
        remainData -= sentBytes;
        fprintf (stdout, "Server sent %d bytes from file's data, offset is now: %ld and remaining data = %d\n", sentBytes, offset, remainData);
    }

}

// This handles the connection for each new client that connects
void *connectionHandler (void *peerSocket) {

    int peer = *(int *) peerSocket;

    // send welcome message
    send (peer, welcome, sizeof (welcome), 0);

    close (peer);
    free (peerSocket);

}

int main (void) {

    fprintf (stdout, "\n---> Blackrock Server <---\n\n");

    struct sockaddr_in serverAddress;
    serverSocket = initServer (serverAddress);

    // get the file data
    struct stat fileStats;
    char fileSize[256];
    int fd = fileData (filepath, fileStats, fileSize);

    // we can now listen for connections
    fprintf (stdout, "Waiting for connections...\n\n");
    listen (serverSocket, 5);   // 5 is our number of connections

    socklen_t sockLen = sizeof (struct sockaddr_in);

    // accepting peers
    struct sockaddr_in peerAddress;
    int peerSocket;
    int *newSocket = NULL;

    while ((peerSocket = accept (serverSocket, (struct sockaddr *) &peerAddress, &sockLen))){
        fprintf(stdout, "Peer connected: %s\n", inet_ntoa (peerAddress.sin_addr));

        pthread_t peerThread;
        newSocket = (int *) malloc (sizeof (int));
        *newSocket = peerSocket;

        if (pthread_create (&peerThread, NULL, connectionHandler, newSocket) < 0)
            fprintf (stderr, "Error creating peer thread!\n");

        else printf ("Handler assigned.\n");

        if (pthread_join (peerThread, NULL) < 0) fprintf (stderr, "Error joinning peer thread!\n");
        else fprintf (stdout, "Client disconnected.\n");
    }

    if (peerSocket < 0) error ("Error accepting connection!\n");

    // close the socket when we are done
    close (clientSocket);
    close (serverSocket);

    return 0;

}