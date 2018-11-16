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

// takes no argument and returns a value (int)
typedef u8 (*Func)(void);
// takes an argument and does not return a value
typedef void (*Action)(void *);
// takes an argument and returns a value (int)
typedef u8 (*delegate)(void *);

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

typedef uint16_t PlayerId;

typedef struct Player {

	struct _Client *client;		// client network data associated to this player

	PlayerId id;
	bool inLobby;

	PlayerInput input;
	u32 inputSequenceNum;
	// TimeSpec lastInputTime;

	bool alive;

	// 15/11/2018 -- we spec the player to be ecs based
	// the server admin can add its own components
	void *components;

} Player;

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

	// 15/11/2018 - the server admin can add its server specific data types
	void *gameData;
	// TODO: maybe add a ptr to a game data destroy function?

};

typedef struct _Lobby Lobby;

// 10/11/2018 - aux reference to a server and lobby for thread functions
typedef struct ServerLobby {

    struct _Server *server;
    Lobby *lobby;

} ServerLobby;

/*** GAME SERVER FUNCTIONS ***/

extern void gs_add_gameInit (struct _Server *server, GameType gameType, delegate *gameInit);
extern void gs_add_loadGameData (struct _Server *server, Func loadData);

extern void gs_handlePacket (struct _PacketInfo *packet);

/*** SCORES ***/

#include "utils/htab.h"

#define DEFAULT_SCORE_SIZE      5   // default players inside the scoreboard

typedef struct ScoreBoard {

    Htab *scores;
    u8 registeredPlayers;
    u8 scoresNum;
    char **scoreTypes;

} ScoreBoard;

extern Htab *game_score_new (u8 initSize);

extern u8 game_score_add_player (ScoreBoard *sb, char *playerName);
extern u8 game_score_remove_player (ScoreBoard *sb, char *playerName);

extern i32 game_score_get (ScoreBoard *sb, char *playerName, char *scoreType);
extern void game_score_set (ScoreBoard *sb, char *playerName, char *scoreType, i32 value);
extern void game_score_update (ScoreBoard *sb, char *playerName, char *scoreType, i32 value);

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

/*** GAME SERIALIZATION ***/

#pragma region GAME SERIALIZATION

typedef struct Splayer {


} SPlayer;

typedef struct UpdatedGamePacket {

	GameSettings gameSettings;

	PlayerId playerId;

	SequenceNum sequenceNum;
	SequenceNum ack_input_sequence_num;

	// SArray players; // Array of SPlayer.
	// SArray explosions; // Array of SExplosion.
	// SArray projectiles; // Array of SProjectile.

} UpdatedGamePacket; 

typedef struct PlayerInput {

	Position pos;

} PlayerInput;

typedef uint64_t SequenceNum;

typedef struct PlayerInputPacket {

	SequenceNum sequenceNum;
	PlayerInput input;
	
} PlayerInputPacket;

/* typedef struct SPlayer {
	SPlayerId id;
	// SBool alive;
	// SVectorFloat position;
	float heading;
	uint32_t score;
	// SColor color;
} SPlayer; */

// in the game we move square by square
typedef struct Position {

    u8 x, y;
    // u8 layer;   

} Position;

#pragma endregion

#endif