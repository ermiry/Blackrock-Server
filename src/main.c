#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <signal.h>


/*** THREAD ***/

// TODO: maybe handle this in a separate list by a name?
Server *gameServer = NULL;

// correctly closes any on-going server and process when quitting the appplication
void closeProgram (int dummy) {

    if (gameServer) cerver_teardown (gameServer);
    else logMsg (stdout, NO_TYPE, NO_TYPE, "There isn't any server to teardown. Quitting application.");

}

// FIXME: how can we signal the process to end?
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

    

    // if we reach this point, be sure to correctly clean all of our data...
    closeProgram (0);
    
    return 0;

}