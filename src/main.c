#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <signal.h>

#include <errno.h>

#include "cerver.h"

#include "utils/myUtils.h"
#include "utils/thpool.h"
#include "utils/log.h"

/*** THREAD ***/

// TODO: maybe handle this in a separate list by a name?
Server *gameServer = NULL;

// correctly closes any on-going server and process when quitting the appplication
void closeProgram (int dummy) {

    if (gameServer) cerver_teardown (gameServer);
    else logMsg (stdout, NO_TYPE, NO_TYPE, "There isn't any server to teardown. Quitting application.");

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, NO_TYPE, "Clearing thread pool...");
    #endif

    thpool_destroy (thpool);

}

// TODO: where do we want to put this?
void *authPacket;
size_t authPacketSize;

threadpool thpool;

// TODO: recieve signals to init, retsart or teardown a server -> like a control panel
int main (void) {

    // register to the quit signal
    signal (SIGINT, closeProgram);

    // 21/20/2018 -- propsed blackrock cerver
    /***
     * create a load balancer listening on a port
     * create 2 virtal game servers, one file server, and one master server
     * the ld decides where to send the incomming requests
     * the 3 servers need to sync to the master server
     * the master server can't be access from any direct request
    **/

    thpool = thpool_init (4);

    // 22/10/2018 -- TODO: where do we want to put this?
    authPacket = generateClientAuthPacket ();
    authPacketSize = sizeof (PacketHeader) + sizeof (RequestData);
    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, NO_TYPE, createString ("Auth packet size: %i.", authPacketSize));
    #endif

    gameServer = cerver_createServer (NULL, GAME_SERVER, destroyGameServer);
    if (gameServer) {
        if (!cerver_startServer (gameServer)) {
            // TODO: log which server
            logMsg (stdout, SUCCESS, SERVER, "Server has started!");
            logMsg (stdout, DEBUG_MSG, SERVER, "Waiting for connections...");

            // FIXME: we don't want to code this here manually!!
            // start the poll loop
            int poll_retval;    // ret val from poll function
            int currfds;        // copy of n active server poll fds

            int newfd;          // fd of new connection
            while (gameServer->isRunning) {
                poll_retval = poll (gameServer->fds, gameServer->nfds, gameServer->pollTimeout);

                // poll failed
                if (poll_retval < 0) {
                    logMsg (stderr, ERROR, SERVER, "Poll failed!");
                    perror ("Error");
                    gameServer->isRunning = false;
                    break;
                }

                // if poll has timed out, just continue to the next loop... 
                if (poll_retval == 0) {
                    #ifdef DEBUG
                    logMsg (stdout, DEBUG_MSG, SERVER, "Poll timeout.");
                    #endif
                    continue;
                }

                // one or more fd(s) are readable, need to determine which ones they are
                currfds = gameServer->nfds;
                for (u8 i = 0; i < currfds; i++) {
                    if (gameServer->fds[i].revents == 0) continue;

                    // FIXME: how to hanlde an unexpected result??
                    if (gameServer->fds[i].revents != POLLIN) {
                        // TODO: log more detailed info about the fd, or client, etc
                        // printf("  Error! revents = %d\n", fds[i].revents);
                        logMsg (stderr, ERROR, SERVER, "Unexpected poll result!");
                    }

                    // listening fd is readable (sever socket)
                    if (gameServer->fds[i].fd == gameServer->serverSock) {
                        //  printf("  Listening socket is readable\n");

                        // accept incoming connections that are queued
                        do {
                            newfd = accept (gameServer->serverSock, NULL, NULL);

                            if (newfd < 0) {
                                // if we get EWOULDBLOCK, we have accepted all connections
                                if (errno != EWOULDBLOCK) {
                                    // if not, accept failed
                                    // FIXME: how to handle this??
                                    logMsg (stderr, ERROR, SERVER, "Accept failed!");
                                }
                            }

                            // we have a new connection
                            // FIXME: try merging the logic with my own accept function
                                // we want to get a new thread from th poll to authenticate the client
                            gameServer->fds[gameServer->nfds].fd = newfd;
                            gameServer->fds[gameServer->nfds].events = POLLIN;
                            gameServer->nfds++;

                            #ifdef DEBUG
                            logMsg (stdout, DEBUG_MSG, SERVER, "Accepted a new connection.");
                            #endif
                        } while (newfd != -1);
                    }

                    // TODO: do we want to hanlde this using a separte thread???
                    // not the server socket, so a connection fd must be readable
                    else {
                        size_t rc;          // retval from recv
                        char buffer[1024];  // buffer for data recieved from fd
                        // TODO: better handlde the lenght of this buffer!

                        // printf("  Descriptor %d is readable\n", fds[i].fd);
                        // recive all incoming data from this socket
                        do {
                            rc = recv (gameServer->fds[i].fd, buffer, sizeof (buffer), 0);
                            
                            // recv error
                            if (rc < 0) {
                                if (errno != EWOULDBLOCK) {
                                    // FIXME: better handle this error!
                                    logMsg (stderr, ERROR, SERVER, "Recv failed!");
                                    perror ("Error:");
                                }
                            }

                            // check if the connection has been closed by the client
                            if (rc == 0) {
                                #ifdef DEBUG
                                logMsg (stdout, DEBUG_MSG, SERVER, "Client closed the connection.");
                                #endif
                                // TODO: what to do next?
                            }


                            // FIXME: handle the request/packet from the client
                            // data was recieved
                            // len = rc;
                            // printf("  %d bytes received\n", len);
                        } while (true);

                        // FIXME: set the end connection flag in the recv loop
                        // end the connection if we were indicated
                        /* if (close_conn) {
                            close(fds[i].fd);
                            fds[i].fd = -1;
                            compress_array = TRUE;
                        } */
                    }

                }

                // FIXME: if we removed a file descriptor, we need to compress the pollfd array

                // FIXME: when we close a connection, we need to delete it from the fd array
                    // this happens when a client disconnects or we teardown the server...

            }
        }
    } 

    // if we reach this point, be sure to correctly clean all of our data...
    closeProgram (0);

    return 0;

}