#include <stdlib.h>
#include <string.h>

#include "cerver/errors.h"
#include "cerver/packets.h"

Error *error_new (ErrorType error_type, const char *msg) {

    Error *error = (Error *) malloc (sizeof (Error));
    if (error) {
        memset (error, 0, sizeof (Error));
        error->error_type = error_type;
        error->msg = msg ? str_new (msg) : NULL;
    }

    return error;

}

void error_delete (void *ptr) {

    if (ptr) {
        Error *error = (Error *) ptr;
        str_delete (error->msg);
        free (error);
    }

}

SError error_serialize (Error *error) {

    if (error) {
        SError serror;
        serror.error_type = error->error_type;
        memset (serror.msg, 0, 64);
        strncpy (serror.msg, error->msg, 64);

        return serror;
    }

}

// creates an error packet ready to be sent
Packet *error_packet_generate (ErrorType error_type, const char *msg) {

    Error *error = error_new (error_type, msg);
    SError serror = error_serialize (error);

    Packet *packet = packet_create (ERROR_PACKET, &serror, sizeof (SError));

    error_delete (error);
    
    return packet;

}