#ifndef _CERVER_GAME_H_
#define _CERVER_GAME_H_

#include <poll.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/cerver.h"

#include "cerver/game/player.h"
#include "cerver/game/lobby.h"

#include "cerver/collections/dllist.h"
#include "cerver/collections/avl.h"

#include "cerver/utils/config.h"
#include "cerver/utils/objectPool.h"

struct _Cerver;
struct _GameServerData;
struct _Client;
struct _PacketInfo;

#define DEFAULT_PLAYER_TIMEOUT      30
#define DEFAULT_FPS                 20
#define DEFAULT_MIN_PLAYERS         2
#define DEFAULT_MAX_PLAYERS         4    

struct _GameCerver {

    // TODO: better arrange the lobbys based on types
    DoubleList *current_lobbys;                     // a list of the current lobbys
    void *(*lobby_id_generator) (const void *);

    Comparator player_comparator;

    // we can define a function to load game data at start, 
    // for example to connect to a db or something like that
    Action load_game_data;
    Action delete_game_data;
    void *game_data;

    // action to be performed right before the game server teardown
    Action final_game_action;
    void *final_action_args;

};

typedef struct _GameCerver GameCerver;

extern GameCerver *game_new (void);

extern void game_delete (void *game_ptr);

/*** Game Cerver Configuration ***/

// option to set the game cerver lobby id generator
extern void game_set_lobby_id_generator (GameCerver *game_cerver, 
    void *(*lobby_id_generator) (const void *));

// option to set the game cerver comparator
extern void game_set_player_comparator (GameCerver *game_cerver, 
    Comparator player_comparator);

// sets a way to get and destroy game cerver game data
extern void game_set_load_game_data (GameCerver *game_cerver, 
    Action load_game_data, Action delete_game_data);

// option to set an action to be performed right before the game cerver teardown
// eg. send a message to all players
extern void game_set_final_action (GameCerver *game_cerver, 
    Action final_action, void *final_action_args);


/*** THE FOLLOWING AND KIND OF BLACKROCK SPECIFIC ***/
/*** WE NEED TO DECIDE WITH NEED TO BE ON THE FRAMEWORK AND WHICH DOES NOT!! ***/

// 17/11/2018 -- aux structure for traversing a players tree
typedef struct PlayerAndData {

    void *playerData;
    void *data;

} PlayerAndData;

/*** LOG_GAME PACKETS ***/

// 04/11/2018 -- 21:29 - to handle requests from players inside the lobby
// primarilly game updates and messages
typedef struct GamePacketInfo {

    struct _Cerver *server;
    Lobby *lobby;
    Player *player;
    char packetData[65515];		// max udp packet size
    size_t packetSize;

} GamePacketInfo;

/*** LOG_GAME SERIALIZATION ***/

#pragma region LOG_GAME SERIALIZATION

typedef int32_t SRelativePtr;
struct _SArray;

// TODO: 19/11/2018 -- 19:05 - we need to add support for scores!

// info that we need to send to the client about the players
typedef struct Splayer {

	// TODO:
	// char name[64];

	// TODO: 
	// we need a way to add info about the players info for specific game
	// such as their race or level in blackrock

	bool owner;

} SPlayer;

// info that we need to send to the client about the lobby he is in
typedef struct SLobby {

	GameSettings settings;
    bool inGame;

	// array of players inside the lobby
	// struct _SArray players;

} SLobby;

// FIXME: do we need this?
typedef struct UpdatedGamePacket {

	GameSettings gameSettings;

	PlayerId playerId;

	// SequenceNum sequenceNum;
	// SequenceNum ack_input_sequence_num;

	// SArray players; // Array of SPlayer.
	// SArray explosions; // Array of SExplosion.
	// SArray projectiles; // Array of SProjectile.

} UpdatedGamePacket; 

// FIXME: do we need this?
typedef struct PlayerInput {

	// Position pos;

} PlayerInput;

typedef uint64_t SequenceNum;

// FIXME:
typedef struct PlayerInputPacket {

	// SequenceNum sequenceNum;
	// PlayerInput input;
	
} PlayerInputPacket;

// FIXME: this should be a serialized version of the game component
// in the game we move square by square
/* typedef struct Position {

    u8 x, y;
    // u8 layer;   

} Position; */

#pragma endregion

#pragma region new 

struct _Packet;

#include "cerver/packets.h"

// handles a game type packet
extern void game_packet_handler (struct _Packet *packet);

#pragma endregion

#endif