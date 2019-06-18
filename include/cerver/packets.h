#ifndef _CERVER_PACKETS_H_
#define _CERVER_PACKETS_H_

#include <stdlib.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/cerver.h"
#include "cerver/client.h"

struct _Server;
struct _Client;

typedef u32 ProtocolID;

extern void packets_set_protocol_id (ProtocolID protocol_id);

typedef struct ProtocolVersion {

	u16 major;
	u16 minor;
	
} ProtocolVersion;

extern void packets_set_protocol_version (ProtocolVersion version);

// these indicate what type of packet we are sending/recieving
typedef enum PacketType {

    SERVER_PACKET = 0,
    CLIENT_PACKET,
    ERROR_PACKET,
	REQUEST_PACKET,
    AUTH_PACKET,
    GAME_PACKET,

    APP_ERROR_PACKET,
    APP_PACKET,

    CUSTOM_PACKET = 70,

    TEST_PACKET = 100,
    DONT_CHECK_TYPE,

} PacketType;

typedef struct PacketHeader {

	ProtocolID protocol_id;
	ProtocolVersion protocol_version;
	PacketType packet_type;
    size_t packet_size;

} PacketHeader;

// these indicate the data and more info about the packet type
typedef enum RequestType {

    SERVER_INFO = 0,
    SERVER_TEARDOWN,

    CLIENT_DISCONNET,

    REQ_GET_FILE,
    POST_SEND_FILE,
    
    REQ_AUTH_CLIENT,
    CLIENT_AUTH_DATA,
    SUCCESS_AUTH,

    LOBBY_CREATE,
    LOBBY_JOIN,
    LOBBY_LEAVE,
    LOBBY_UPDATE,
    LOBBY_DESTROY,

    GAME_INIT,      // prepares the game structures
    GAME_START,     // strat running the game
    GAME_INPUT_UPDATE,
    GAME_SEND_MSG,

} RequestType;

typedef struct RequestData {

    RequestType type;

} RequestData;

typedef struct Packet {

    // the cerver and client the packet is from
    struct _Server *server;
    struct _Client *client;

    PacketType packet_type;
    String *custom_type;

    // serilized data
    size_t data_size;
    void *data;

    // the actual packet to be sent
    PacketHeader *header;
    size_t packet_size;
    void *packet;

} Packet;

extern Packet *packet_new (void);

extern void packet_delete (void *ptr);

// prepares the packet to be ready to be sent
extern u8 packet_generate (Packet *packet);

// sends a packet using the tcp protocol
extern u8 packet_send_tcp (i32 socket_fd, const void *packet, size_t packet_size, int flags);

// sends a packet using the udp protocol
extern u8 packet_send_udp (const void *packet, size_t packet_size);

#endif