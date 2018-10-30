#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <signal.h>

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
        if (!cerver_startServer (gameServer)) 
            logMsg (stdout, SUCCESS, SERVER, "Server started properly!");

        else logMsg (stderr, ERROR, SERVER, "Failed to start server!");
    } 

    // if we reach this point, be sure to correctly clean all of our data...
    closeProgram (0);

    return 0;

}