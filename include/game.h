#ifndef GAME_SERVER_H
#define GAME_SERVER_H

#include "cerver.h"

// #include "utils/myTime.h"
// #include "utils/vector.h"

#include <poll.h>
#include "utils/avl.h"

/*** CERVER TYPES ***/

// 11/11/2018 -- added this types defs here to prevent compiler erros

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

struct _Server;
struct _GameServerData;
struct _Client;
struct _PacketInfo;

#define DEFAULT_PLAYER_TIMEOUT      30
#define DEFAULT_FPS                 20
#define DEFAULT_MIN_PLAYERS         2
#define DEFAULT_MAX_PLAYERS         4    

#define FPS		20

typedef enum GameType {

	ARCADE = 0

} GameType;

// TODO: what other settings do we need?? map? enemies? loot?
struct _GameSettings {

	GameType gameType;

	u8 playerTimeout; 	// in seconds.
	u8 fps;

	u8 minPlayers;
	u8 maxPlayers;

	// duration?

};

typedef struct _GameSettings GameSettings;

// in the game we move square by square
typedef struct Position {

    u8 x, y;
    // u8 layer;   

} Position;

typedef struct PlayerInput {

	Position pos;

} PlayerInput;

/* typedef struct SPlayer {
	SPlayerId id;
	// SBool alive;
	// SVectorFloat position;
	float heading;
	uint32_t score;
	// SColor color;
} SPlayer; */

typedef uint64_t SequenceNum;

typedef struct PlayerInputPacket {

	SequenceNum sequenceNum;
	PlayerInput input;
	
} PlayerInputPacket;

typedef uint16_t PlayerId;

typedef struct UpdatedGamePacket {

	GameSettings gameSettings;

	PlayerId playerId;

	SequenceNum sequenceNum;
	SequenceNum ack_input_sequence_num;

	// SArray players; // Array of SPlayer.
	// SArray explosions; // Array of SExplosion.
	// SArray projectiles; // Array of SProjectile.

} UpdatedGamePacket; 

// TODO: maybe add the game components here? as in the client?
typedef struct Player {

	struct _Client *client;		// client network data associated to this player

	PlayerId id;
	bool inLobby;

	PlayerInput input;
	u32 inputSequenceNum;
	// TimeSpec lastInputTime;

	bool alive;

	// Components
	Position pos;

} Player;

typedef struct Level {

    u8 levelNum;
    bool **mapCells;    // dungeon map

} Level;

// TODO: move this from here for a cleaner code
// in game data structres
typedef struct World {

	Level *level;

} World;

struct _Lobby {

	GameSettings *settings;
	bool inGame;

	Player *owner;				// the client that created the lobby -> he has higher privileges
	// Vector players;			// the clients connected to the lobby

	bool isRunning;				// lobby is listening for player packets

	// 04/11/2018 -- lets try this and see how it goes - intended to also work for a bigger 
	// lobby with more players in it
	AVLTree *players;							// players inside the lobby
    struct pollfd players_fds[4];     			// 04/11/2018 - 4 max players in lobby
    u16 players_nfds;                           // n of active fds in the pollfd array
    bool compress_players;              		// compress the fds array?
    u32 pollTimeout;    

	World *world;			// in game data structres

};

typedef struct _Lobby Lobby;

/*** GAME SERVER FUNCTIONS ***/

extern void gs_add_gameInit (Server *server, GameType gameType, delegate *gameInit);

extern void gs_handlePacket (struct _PacketInfo *packet);

/*** GAME PACKETS ***/

// 04/11/2018 -- 21:29 - to handle requests from players inside the lobby
// primarilly game updates and messages
typedef struct GamePacketInfo {

    struct _Server *server;
    Lobby *lobby;
    Player *player;
    char packetData[65515];		// max udp packet size
    size_t packetSize;

} GamePacketInfo;

/*** SERIALIZATION ***/

// game serialized data

typedef struct Splayer {


} SPlayer;

#endif