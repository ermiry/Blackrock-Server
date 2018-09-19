// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

// #include <sys/types.h>
// #include <unistd.h>

// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <netinet/in.h>

// #include <sys/sendfile.h>
// #include <sys/stat.h>
// #include <fcntl.h>

// #include <errno.h>

// #define PORT    9001

// // #define SERVER_ADDRESS  "192.168.1.7"

// char welcome[256] = "You have reached the Blackrock Server!!";

// int serverSocket;
// int clientSocket;

// void error (const char *msg) {

//     perror (msg);

//     close (serverSocket);
//     close (clientSocket);

//     exit (1);

// }

// void sendFile () {



// }

// int main (void) {

//     fprintf (stdout, "\n---> Blackrock Server <---\n\n");

//     // create the server socket
//     serverSocket = socket (AF_INET, SOCK_STREAM, 0);
//     if (serverSocket < 0) error ("Error creating server socket!\n");

//     // define the server address
//     struct sockaddr_in serverAddress;
//     // clears any data in server address
//     bzero ((char *) &serverAddress, sizeof (serverAddress));
//     serverAddress.sin_family = AF_INET;
//     serverAddress.sin_port = htons (PORT);  // arbitrary port for now
//     serverAddress.sin_addr.s_addr = INADDR_ANY;     // ip = 0.0.0.0
//     // inet_pton(AF_INET, SERVER_ADDRESS, &(server_addr.sin_addr));

//     // bind the socket to our specify IP and port
//     int binding = bind (serverSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress));
//     if (binding < 0) error ("Error binding server socket!\n");

//     // we can now listen for connections
//     fprintf (stdout, "Waiting for connection..\n\n");
//     struct sockaddr_in clientAddress;
//     listen (serverSocket, 5);   // 5 is our number of connections
//     socklen_t clientLen = sizeof (clientAddress);

//     // prepare file to send
//     int fd = open ("./data/test.txt", O_RDONLY);
//     if (fd < 0) fprintf (stderr, "Error openning file!\n%s\n", strerror (errno));

//     // get file stats
//     struct stat fileStats;
//     if (fstat (fd, &fileStats) < 0) 
//         fprintf (stderr, "Error openning file!\n%s\n", strerror (errno));

//     fprintf (stdout, "\n\nFile size: %ld bytes.\n\n", fileStats.st_size);

//     // accpet a connection
//     clientSocket = accept (serverSocket, (struct sockaddr *) &clientAddress, &clientLen);
//     if (clientSocket < 0) error ("Error accepting new connection!\n");
//     else {
//         fprintf (stdout, "Client connected!\n");
//         fprintf (stdout, "%s\n", inet_ntoa (clientAddress.sin_addr));
//     } 

//     // send the welcome message if the connection is successfull
//     send (clientSocket, welcome, sizeof (welcome), 0);

//     char fileSize[256];
//     sprintf (fileSize, "%ld", fileStats.st_size);

//     // sending file size
//     size_t len = send (clientSocket, fileSize, sizeof (fileSize), 0);
//     if (len < 0) fprintf (stderr, "Error sending file size!\n%s\n", strerror (errno));

//     fprintf (stdout, "Server sent %ld bytes for the size.\n", len);

//     // sending file data
//     off_t offset = 0;
//     int remainData = fileStats.st_size;
//     int sentBytes = 0;

//     while (((sentBytes = sendfile (clientSocket, fd, &offset, BUFSIZ)) > 0) && (remainData > 0)) {
//         fprintf (stdout, "Server sent %d bytes from file's data, offset is now: %ld and remaining data = %d\n", sentBytes, offset, remainData);
//         remainData -= sentBytes;
//         fprintf (stdout, "Server sent %d bytes from file's data, offset is now: %ld and remaining data = %d\n", sentBytes, offset, remainData);
//     }

//     // close the socket when we are done
//     close (clientSocket);
//     close (serverSocket);

//     return 0;

// }

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#define PORT_NUMBER     9005
// #define SERVER_ADDRESS  "192.168.1.7"
// #define FILE_TO_SEND    "hello.c"

int main(int argc, char **argv)
{
        int server_socket;
        int peer_socket;
        socklen_t       sock_len;
        ssize_t len;
        struct sockaddr_in      server_addr;
        struct sockaddr_in      peer_addr;
        int fd;
        int sent_bytes = 0;
        char file_size[256];
        struct stat file_stat;
        off_t offset;
        int remain_data;

        /* Create server socket */
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == -1)
        {
                fprintf(stderr, "Error creating socket --> %s", strerror(errno));

                exit(EXIT_FAILURE);
        }

        /* Zeroing server_addr struct */
        memset(&server_addr, 0, sizeof(server_addr));
        /* Construct server_addr struct */
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;     // ip = 0.0.0.0
        // inet_pton(AF_INET, SERVER_ADDRESS, &(server_addr.sin_addr));
        server_addr.sin_port = htons(PORT_NUMBER);

        /* Bind */
        if ((bind(server_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))) == -1)
        {
                fprintf(stderr, "Error on bind --> %s", strerror(errno));

                exit(EXIT_FAILURE);
        }

        /* Listening to incoming connections */
        if ((listen(server_socket, 5)) == -1)
        {
                fprintf(stderr, "Error on listen --> %s", strerror(errno));

                exit(EXIT_FAILURE);
        }

        fd = open("./data/test.txt", O_RDONLY);
        if (fd == -1)
        {
                fprintf(stderr, "Error opening file --> %s", strerror(errno));

                exit(EXIT_FAILURE);
        }

        /* Get file stats */
        if (fstat(fd, &file_stat) < 0)
        {
                fprintf(stderr, "Error fstat --> %s", strerror(errno));

                exit(EXIT_FAILURE);
        }

        fprintf(stdout, "File Size: \n%ld bytes\n", file_stat.st_size);

        sock_len = sizeof(struct sockaddr_in);
        /* Accepting incoming peers */
        peer_socket = accept(server_socket, (struct sockaddr *)&peer_addr, &sock_len);
        if (peer_socket == -1)
        {
                fprintf(stderr, "Error on accept --> %s", strerror(errno));

                exit(EXIT_FAILURE);
        }
        fprintf(stdout, "Accept peer --> %s\n", inet_ntoa(peer_addr.sin_addr));

        sprintf(file_size, "%ld", file_stat.st_size);

        /* Sending file size */
        len = send(peer_socket, file_size, sizeof(file_size), 0);
        if (len < 0)
        {
              fprintf(stderr, "Error on sending greetings --> %s", strerror(errno));

              exit(EXIT_FAILURE);
        }

        fprintf(stdout, "Server sent %ld bytes for the size\n", len);

        offset = 0;
        remain_data = file_stat.st_size;
        /* Sending file data */
        while (((sent_bytes = sendfile(peer_socket, fd, &offset, BUFSIZ)) > 0) && (remain_data > 0))
        {
                fprintf(stdout, "1. Server sent %d bytes from file's data, offset is now : %ld and remaining data = %d\n", sent_bytes, offset, remain_data);
                remain_data -= sent_bytes;
                fprintf(stdout, "2. Server sent %d bytes from file's data, offset is now : %ld and remaining data = %d\n", sent_bytes, offset, remain_data);
        }

        close(peer_socket);
        close(server_socket);

        return 0;
}