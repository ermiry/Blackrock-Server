#include "cerver/types/types.h"

#include "cerver/network.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/sha-256.h"

// TODO: refactor to use timestamps to generate the token
// create a unique session id for each client based on connection values
char *session_default_generate_id (i32 fd, const struct sockaddr_storage address) {

    char *ipstr = sock_ip_to_string ((const struct sockaddr *) &address);
    u16 port = sock_ip_port ((const struct sockaddr *) &address);

    if (ipstr && (port > 0)) {
        // 24/11/2018 -- 22:14 -- testing a simple id - just ip + port
        char *connection_values = c_string_create ("%s-%i", ipstr, port);

        uint8_t hash[32];
        char hash_string[65];

        sha_256_calc (hash, connection_values, strlen (connection_values));
        sha_256_hash_to_string (hash_string, hash);

        char *retval = c_string_create ("%s", hash_string);

        return retval;
    }

    return NULL;

}