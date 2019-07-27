#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <signal.h>

#include "cerver/cerver.h"
#include "cerver/utils/log.h"

#include "ermiry/ermiry.h"

Cerver *black_cerver = NULL;

// correctly closes any on-going server and process when quitting the appplication
void end (int dummy) {
    
    if (black_cerver) cerver_teardown (black_cerver);

    ermiry_end ();

    exit (0);

}

int main (void) {

    // register to the quit signal
    signal (SIGINT, end);

    if (!ermiry_init ()) {
        black_cerver = cerver_create (CUSTOM_CERVER, "black-cerver", 
            7001, PROTOCOL_TCP, false, 2, 2000);
        if (black_cerver) {
            // set cerver configuration
            cerver_set_thpool_n_threads (black_cerver, 4);
            // cerver_set_app_handlers (black_cerver, black_packet_handler, NULL);
            // cerver_set_on_client_connected (black_cerver, black_on_new_client_action);

            if (cerver_start (black_cerver)) {
                cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE,
                    "Failed to start black cerver!");
            }
        }

        else {
            cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
                "Failed to create black cerver!");
        }

        ermiry_end ();
    }

    else {
        cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE,
            "Failed to init black!");
    }

    return 0;

}