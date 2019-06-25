#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <signal.h>

#include "cerver/cerver.h"
#include "cerver/utils/log.h"

#include "ermiry/ermiry.h"
#include "blackrock/blackrock.h"

// correctly closes any on-going server and process when quitting the appplication
void closeProgram (int dummy) {
    
    // FIXME: change this to destroy all cervers!!!
    // if (gameServer) cerver_teardown (gameServer);
    // else cerver_log_msg (stdout, LOG_NO_TYPE, LOG_NO_TYPE, "There isn't any server to teardown. Quitting application.");

    ermiry_end ();      // disconnect from ermiry's db

    exit (0);

}

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

    Cerver *black_server = cerver_create (GAME_CERVER, "Black-Server", 
		7007, PROTOCOL_TCP, false, 10, 180000);
	if (black_server) {

	}

	else {
		cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to create black server!");
	}

    // gameServer = cerver_create (GAME_SERVER, "Black-Game-Server", NULL);
    // if (gameServer) {

    //     // set our own functions to authenticate our clients
    //     cerver_set_auth_method (gameServer, blackrock_authMethod);

    //     // FIXME:
    //     // set our own functions to load and delete global game data
    //     // game_set_load_game_data (gameServer->serverData, blackrock_loadGameData);
    //     // game_set_delete_game_data (gameServer->serverData, blackrock_deleteGameData);

    //     // set blackrock arcade game init function
    //     // FIXME: how can we add our own game types?? check user game components in cengine
    //     // gs_add_gameInit (gameServer, ARCADE, blackrock_init_arcade);

    //     // FIXME:
    //     // set lobby game data destroy function, to delete our world and
    //     // any other data that blackrock lobby uses
    //     // gs_set_lobbyDeleteGameData (gameServer, deleteBrGameData);

    //     if (cerver_start (gameServer)) 
    //         cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Failed to start server!");
    // } 

    // else cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to create Blackrock Server!");

    ermiry_end ();      // disconnect from ermiry's db

    return 0;

}