#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <signal.h>

#include "cerver/cerver.h"

#include "utils/log.h"

#include "ermiry/ermiry.h"
#include "blackrock/blackrock.h"

// TODO: maybe handle this in a separate list by a name?
Server *gameServer = NULL;

// correctly closes any on-going server and process when quitting the appplication
void closeProgram (int dummy) {
    
    if (gameServer) cerver_teardown (gameServer);
    else logMsg (stdout, NO_TYPE, NO_TYPE, "There isn't any server to teardown. Quitting application.");

    ermiry_end ();      // disconnect from ermiry's db

    exit (0);

}

// TODO: recieve signals to init, retsart or teardown a server -> like a control panel
int main (void) {

    // register to the quit signal
    signal (SIGINT, closeProgram);

    // connect to ermiry's db
    ermiry_init ();

    // 21/10/2018 -- propsed blackrock cerver
    /***
     * create a load balancer listening on a port
     * create 2 virtal game servers, one file server, and one master server
     * the ld decides where to send the incomming requests
     * the 3 servers need to sync to the master server
     * the master server can't be access from any direct request
    **/

    gameServer = cerver_createServer (NULL, GAME_SERVER, "Black-Game-Server");
    if (gameServer) {

        // set our own functions to authenticate our clients
        cerver_set_auth_method (gameServer, blackrock_authMethod);

        // set our own functions to load and delete global game data
        gs_set_loadGameData (gameServer, blackrock_loadGameData);
        gs_set_deleteGameData (gameServer, blackrock_deleteGameData);

        // set blackrock arcade game init function
        gs_add_gameInit (gameServer, ARCADE, blackrock_init_arcade);

        // set lobby game data destroy function, to delete our world and
        // any other data that blackrock lobby uses
        gs_set_lobbyDeleteGameData (gameServer, deleteBrGameData);

        if (cerver_startServer (gameServer)) 
            logMsg (stderr, ERROR, SERVER, "Failed to start server!");
    } 

    else logMsg (stderr, ERROR, NO_TYPE, "Failed to create Blackrock Server!");

    ermiry_end ();      // disconnect from ermiry's db

    return 0;

}