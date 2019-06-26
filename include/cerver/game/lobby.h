#ifndef _CERVER_GAME_LOBBY_H_
#define _CERVER_GAME_LOBBY_H_

#include <time.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/game/game.h"
#include "cerver/game/gameType.h"
#include "cerver/game/player.h"

#include "cerver/collections/avl.h"

#include "cerver/utils/objectPool.h"

#define LOBBY_DEFAULT_POLL_TIMEOUT			2000
#define DEFAULT_MAX_LOBBY_PLAYERS			4

struct _Cerver;
struct _Player;
struct _GameServerData;

struct _GameSettings {

	// config
	GameType *game_type;
	u8 player_timeout;		// secons until we drop the player 
	u8 fps;

	// rules
	u8 min_players;
	u8 max_players;
	int duration;			// in secconds, -1 for inifinite duration

};

typedef struct _GameSettings GameSettings;

struct _Lobby {

	String *id;							// lobby unique id - generated using the creation timestamp
	time_t creation_time_stamp;

	Htab *sock_fd_player_map;           // maps a socket fd to a player
    struct pollfd *players_fds;     			
	u16 players_nfds;                   // n of active fds in the pollfd array
    bool compress_players;              // compress the fds array?
    u32 poll_timeout;    

	bool running;						// lobby is listening for player packets
	bool in_game;						// lobby is inside a game

	struct _Player *owner;				// the client that created the lobby -> he has higher privileges
	unsigned int max_players;
	unsigned int current_players;

	Action packet_handler;				// lobby packet handler

	void *game_settings;
	Action game_settings_delete;

	// the server admin can add its server specific data types
	void *game_data;
	Action game_data_delete;

	Action update;						// lobby update function to be executed every fps

};

typedef struct _Lobby Lobby;

// lobby constructor
extern Lobby *lobby_new (void);

extern void lobby_delete (void *lobby_ptr);

typedef struct CerverLobby {

    struct _Cerver *cerver;
    Lobby *lobby;

} CerverLobby;

#endif