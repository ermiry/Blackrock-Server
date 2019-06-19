#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/network.h"
#include "cerver/packets.h"
#include "cerver/cerver.h"
#include "cerver/client.h"

static ProtocolID protocol_id = 0;
static ProtocolVersion protocol_version = { 0, 0 };

void packets_set_protocol_id (ProtocolID proto_id) { protocol_id = proto_id; }

void packets_set_protocol_version (ProtocolVersion version) { protocol_version = version; }

static PacketHeader *packet_header_new (PacketType packet_type, size_t packet_size) {

    PacketHeader *header = (PacketHeader *) malloc (sizeof (PacketHeader));
    if (header) {
        memset (header, 0, sizeof (PacketHeader));
        header->protocol_id = protocol_id;
        header->protocol_version = protocol_version;
        header->packet_type = packet_type;
        header->packet_size = packet_size;
    }

    return header;

}

static inline void packet_header_delete (PacketHeader *header) { if (header) free (header); }

Packet *packet_new (void) {

    Packet *packet = (Packet *) malloc (sizeof (Packet));
    if (packet) {
        memset (packet, 0, sizeof (Packet));
        packet->server = NULL;
        packet->client = NULL;

        packet->custom_type = NULL;
        packet->data = NULL;
        packet->header = NULL;  
        packet->packet = NULL;
    }

    return packet;

}

void packet_delete (void *ptr) {

    if (ptr) {
        Packet *packet = (Packet *) ptr;

        packet->server = NULL;
        packet->client = NULL;

        str_delete (packet->custom_type);
        if (packet->data) free (packet->data);
        packet_header_delete (packet->header);
        if (packet->packet) free (packet->packet);

        free (packet);
    }

}

// FIXME:
// prepares the packet to be ready to be sent
u8 packet_generate (Packet *packet) {

    u8 retval = 0;

    if (packet) {
        packet->header = packet_header_new (packet->packet_type, packet->packet_size);

        // create the packet buffer to be sent
    }

    return retval;

}

// TODO: check for errno appropierly
u8 packet_send_tcp (i32 socket_fd, const void *packet, size_t packet_size, int flags) {

    if (packet) {
        ssize_t sent;
        const void *p = packet;
        while (packet_size > 0) {
            sent = send (socket_fd, p, packet_size, flags);
            if (sent < 0) return 1;
            p += sent;
            packet_size -= sent;
        }

        return 0;
    }

    return 1;

}

// FIXME: correctly send an udp packet!!
u8 packet_send_udp (const void *packet, size_t packet_size) {

    ssize_t sent;
    const void *p = packet;
    // while (packet_size > 0) {
    //     sent = sendto (server->serverSock, begin, packetSize, 0, 
    //         (const struct sockaddr *) &address, sizeof (struct sockaddr_storage));
    //     if (sent <= 0) return -1;
    //     p += sent;
    //     packetSize -= sent;
    // }

    return 0;

}

// FIXME:
/*u8 packet_send (Connection *connection, Packet *packet, int flags) {

    u8 retval = 1;

    if (connection && packet) {
        switch (connection->protocol) {
            case PROTOCOL_TCP: 
                retval = packet_send_tcp (connection->sock_fd, 
                    packet->packet, packet->packet_size, flags);
                break;
            case PROTOCOL_UDP:
                break;

            default: break;
        }
    }

    return retval;

} */

// FIXME:
// check for packets with bad size, protocol, version, etc
u8 packet_check (Packet *packet) {

    /*if (packetSize < sizeof (PacketHeader)) {
        #ifdef CLIENT_DEBUG
        cengine_log_msg (stderr, WARNING, NO_TYPE, "Recieved a to small packet!");
        #endif
        return 1;
    } 

    PacketHeader *header = (PacketHeader *) packetData;

    if (header->protocolID != PROTOCOL_ID) {
        #ifdef CLIENT_DEBUG
        logMsg (stdout, WARNING, PACKET, "Packet with unknown protocol ID.");
        #endif
        return 1;
    }

    Version version = header->protocolVersion;
    if (version.major != PROTOCOL_VERSION.major) {
        #ifdef CLIENT_DEBUG
        logMsg (stdout, WARNING, PACKET, "Packet with incompatible version.");
        #endif
        return 1;
    }

    // compare the size we got from recv () against what is the expected packet size
    // that the client created 
    if (packetSize != header->packetSize) {
        #ifdef CLIENT_DEBUG
        logMsg (stdout, WARNING, PACKET, "Recv packet size doesn't match header size.");
        #endif
        return 1;
    } 

    if (expectedType != DONT_CHECK_TYPE) {
        // check if the packet is of the expected type
        if (header->packetType != expectedType) {
            #ifdef CLIENT_DEBUG
            logMsg (stdout, WARNING, PACKET, "Packet doesn't match expected type.");
            #endif
            return 1;
        }
    }

    return 0;   // packet is fine */

}

/*** OLD PACKETS CODE ***/

// FIXME: object pool!!
PacketInfo *newPacketInfo (Server *server, Client *client, i32 clientSock,
    char *packetData, size_t packetSize) {

    PacketInfo *p = (PacketInfo *) malloc (sizeof (PacketInfo));

    // if (server->packetPool) {
    //     if (POOL_SIZE (server->packetPool) > 0) {
    //         p = pool_pop (server->packetPool);
    //         if (!p) p = (PacketInfo *) malloc (sizeof (PacketInfo));
    //         else if (p->packetData) free (p->packetData);
    //     }
    // }

    // else {
    //     p = (PacketInfo *) malloc (sizeof (PacketInfo));
    //     p->packetData = NULL;
    // } 

    if (p) {
        p->packetData = NULL;

        p->server = server;

        if (!client) printf ("pack info - got a NULL client!\n");

        p->client = client;
        p->clientSock = clientSock;
        p->packetSize = packetSize;
        
        // copy the contents from the entry buffer to the packet info
        if (!p->packetData)
            p->packetData = (char *) calloc (MAX_UDP_PACKET_SIZE, sizeof (char));

        memcpy (p->packetData, packetData, MAX_UDP_PACKET_SIZE);
    }

    return p;

}

// used to destroy remaining packet info in the pools
void destroyPacketInfo (void *data) {

    if (data) {
        PacketInfo *packet = (PacketInfo *) data;
        packet->server= NULL;
        packet->client = NULL;
        if (packet->packetData) free (packet->packetData);
    }

}

// check for packets with bad size, protocol, version, etc
u8 checkPacket (size_t packetSize, char *packetData, PacketType expectedType) {

    if (packetSize < sizeof (PacketHeader)) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stderr, WARNING, PACKET, "Recieved a to small packet!");
        #endif
        return 1;
    } 

    // our clients must send us a packet with the cerver;s packet header to be valid
    PacketHeader *header = (PacketHeader *) packetData;

    if (header->protocolID != PROTOCOL_ID) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, WARNING, PACKET, "Packet with unknown protocol ID.");
        #endif
        return 1;
    }

    // FIXME: error when sending auth packet from client!
    // Version version = header->protocolVersion;
    // if (version.major != PROTOCOL_VERSION.major) {
    //     #ifdef CERVER_DEBUG
    //     cerver_log_msg (stdout, WARNING, PACKET, "Packet with incompatible version.");
    //     #endif
    //     return 1;
    // }

    // compare the size we got from recv () against what is the expected packet size
    // that the client created 
    if ((u32) packetSize != header->packetSize) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, WARNING, PACKET, "Recv packet size doesn't match header size.");
        cerver_log_msg (stdout, DEBUG_MSG, PACKET, 
            c_string_create ("Recieved size: %i - Expected size %i", packetSize, header->packetSize));
        #endif
        return 1;
    } 

    if (expectedType != DONT_CHECK_TYPE) {
        // check if the packet is of the expected type
        if (header->packetType != expectedType) {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, WARNING, PACKET, "Packet doesn't match expected type.");
            #endif
            return 1;
        }
    } 

    return 0;   // packet is fine

}

// THIS IS ONLY FOR UDP
// FIXME: when the owner of the lobby has quit the game, we ned to stop listenning for that lobby
    // also destroy the lobby and all its mememory
// Also check if there are no players left in the lobby for any reason
// this is used to recieve packets when in a lobby/game
void recievePackets (void) {

    /*unsigned char packetData[MAX_UDP_PACKET_SIZE];

    struct sockaddr_storage from;
    socklen_t fromSize = sizeof (struct sockaddr_storage);
    ssize_t packetSize;

    bool recieve = true;

    while (recieve) {
        packetSize = recvfrom (server, (char *) packetData, MAX_UDP_PACKET_SIZE, 
            0, (struct sockaddr *) &from, &fromSize);

        // no more packets to process
        if (packetSize < 0) recieve = false;

        // process packets
        else {
            // just continue to the next packet if we have a bad one...
            if (checkPacket (packetSize, packetData, PLAYER_INPUT_TYPE)) continue;

            // if the packet passes all the checks, we can use it safely
            else {
                PlayerInputPacket *playerInput = (PlayerInputPacket *) (packetData + sizeof (PacketHeader));
                handlePlayerInputPacket (from, playerInput);
            }

        }        
    } */ 

}

void initPacketHeader (void *header, PacketType type, u32 packetSize) {

    PacketHeader h;
    h.protocolID = PROTOCOL_ID;
    h.protocolVersion = PROTOCOL_VERSION;
    h.packetType = type;
    h.packetSize = packetSize;

    memcpy (header, &h, sizeof (PacketHeader));

}

// generates a generic packet with the specified packet type
void *generatePacket (PacketType packetType, size_t packetSize) {

    size_t packet_size;
    if (packetSize > 0) packet_size = packetSize;
    else packet_size = sizeof (PacketHeader);

    PacketHeader *header = (PacketHeader *) malloc (packet_size);
    initPacketHeader (header, packetType, packet_size); 

    return header;

}

// TODO: 06/11/2018 -- test this!
i8 udp_sendPacket (Server *server, const void *begin, size_t packetSize, 
    const struct sockaddr_storage address) {

    ssize_t sent;
    const void *p = begin;
    while (packetSize > 0) {
        sent = sendto (server->serverSock, begin, packetSize, 0, 
            (const struct sockaddr *) &address, sizeof (struct sockaddr_storage));
        if (sent <= 0) return -1;
        p += sent;
        packetSize -= sent;
    }

    return 0;

}

i8 tcp_sendPacket (i32 socket_fd, const void *begin, size_t packetSize, int flags) {

    ssize_t sent;
    const void *p = begin;
    while (packetSize > 0) {
        sent = send (socket_fd, p, packetSize, flags);
        if (sent <= 0) return -1;
        p += sent;
        packetSize -= sent;
    }

    return 0;

}

// handles the correct logic for sending a packet, depending on the supported protocols
u8 server_sendPacket (Server *server, i32 socket_fd, struct sockaddr_storage address, 
    const void *packet, size_t packetSize) {

    if (server) {
        switch (server->protocol) {
            case IPPROTO_TCP: 
                if (tcp_sendPacket (socket_fd, packet, packetSize, 0) >= 0)
                    return 0;
                break;
            case IPPROTO_UDP:
                if (udp_sendPacket (server, packet, packetSize, address) >= 0)
                    return 0;
                break;
            default: break;
        }
    }

    return 1;

}

// just creates an erro packet -> used directly when broadcasting to many players
void *generateErrorPacket (ErrorType type, const char *msg) {

    size_t packetSize = sizeof (PacketHeader) + sizeof (ErrorData);
    
    void *begin = generatePacket (ERROR_PACKET, packetSize);
    char *end = begin + sizeof (PacketHeader); 

    ErrorData *edata = (ErrorData *) end;
    edata->type = type;
    if (msg) {
        if (strlen (msg) > sizeof (edata->msg)) {
            // clamp the value to fit inside edata->msg
            u16 i = 0;
            while (i < sizeof (edata->msg) - 1) {
                edata->msg[i] = msg[i];
                i++;
            }

            edata->msg[i] = '\0';
        }

        else strcpy (edata->msg, msg);
    }

    return begin;

}

// creates and sends an error packet
u8 sendErrorPacket (Server *server, i32 sock_fd, struct sockaddr_storage address,
    ErrorType type, const char *msg) {

    size_t packetSize = sizeof (PacketHeader) + sizeof (ErrorData);
    void *errpacket = generateErrorPacket (type, msg);

    if (errpacket) {
        server_sendPacket (server, sock_fd, address, errpacket, packetSize);

        free (errpacket);
    }

    return 0;

}

u8 sendTestPacket (Server *server, i32 sock_fd, struct sockaddr_storage address) {

    if (server) {
        size_t packetSize = sizeof (PacketHeader);
        void *packet = generatePacket (TEST_PACKET, packetSize);
        if (packet) {
            u8 retval = server_sendPacket (server, sock_fd, address, packet, packetSize);
            free (packet);

            return retval;
        }
    }

    return 1;

}

void *createClientAuthReqPacket (void) {

    size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData);
    void *begin = generatePacket (AUTHENTICATION, packetSize);
    char *end = begin + sizeof (PacketHeader);

    RequestData *request = (RequestData *) end;
    request->type = REQ_AUTH_CLIENT;

    return begin;

}

// broadcast a packet/msg to all clients inside a client's tree
void broadcastToAllClients (AVLNode *node, Server *server, void *packet, size_t packetSize) {

    if (node && server && packet && (packetSize > 0)) {
        broadcastToAllClients (node->right, server, packet, packetSize);

        // send packet to current client
        if (node->id) {
            Client *client = (Client *) node->id;

            // 02/12/2018 -- send packet to the first active connection
            if (client) 
                server_sendPacket (server, client->active_connections[0], client->address,
                    packet, packetSize);
        }

        broadcastToAllClients (node->left, server, packet, packetSize);
    }

}

void *generateServerInfoPacket (Server *server) {

    if (server) {
        size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData) + sizeof (SServer);
        void *packet = generatePacket (SERVER_PACKET, packetSize);
         (packet + sizeof (RequestData));
        if (packet) {
            char *end = packet + sizeof (PacketHeader);
            RequestData *reqdata = (RequestData *) end;
            reqdata->type = SERVER_INFO;

            end += sizeof (RequestData);

            SServer *sinfo = (SServer *) end;
            sinfo->authRequired = server->authRequired;
            sinfo->port = server->port;
            sinfo->protocol = server->protocol;
            sinfo->type = server->type;
            sinfo->useIpv6 = server->useIpv6;

            return packet;
        }
    }

    return NULL;

}

// send useful server info to the client
void sendServerInfo (Server *server, i32 sock_fd, struct sockaddr_storage address) {

    if (server) {
        if (server->serverInfo) {
            size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData) + sizeof (SServer);

            if (!server_sendPacket (server, sock_fd, address, server->serverInfo, packetSize)) {
                #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Sent server info packet.");
                #endif
            }
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, ERROR, SERVER, "No server info to send to client!");
            #endif
        } 
    }

}

// FIXME:
// TODO: move this from here to the config section
// set an action to be executed when a client connects to the server
void server_set_send_server_info () {}