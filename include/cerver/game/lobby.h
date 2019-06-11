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

#define LOBBY_DEFAULT_POLL_TIMEOUT			180000
#define DEFAULT_MAX_LOBBY_PLAYERS			4

struct _Server;
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

	bool running;						// lobby is listening for player packets
	bool in_game;						// lobby is inside a game

	void *game_settings;
	Action delete_lobby_game_settings;

	struct _Player *owner;				// the client that created the lobby -> he has higher privileges
	AVLTree *players;					// players inside the lobby -> reference to the main player avl
	unsigned int max_players;
	unsigned int current_players;

    struct pollfd *players_fds;     			
    u16 players_nfds;                           // n of active fds in the pollfd array
    bool compress_players;              		// compress the fds array?
    u32 poll_timeout;    

	// the server admin can add its server specific data types
	void *game_data;
	Action delete_lobby_game_data;

	Action handler;						// lobby player handler

};

typedef struct _Lobby Lobby;

// creates a list to manage the server lobbys
// called when we init the game server
// returns 0 on success, 1 on error
extern u8 game_init_lobbys (struct _GameServerData *game_data, u8 n_lobbys);

// lobby constructor
extern Lobby *lobby_new (void);

// initializes a new lobby
// pass the server game data
// pass 0 to max players to use the default 4
// pass NULL in handle to use default
extern Lobby *lobby_init (GameServerData *game_data, unsigned max_players, Action handler);

// deletes a lobby for ever -- called when we teardown the server
// we do not need to give any feedback to the players if there is any inside
extern void lobby_delete (void *ptr);

// used to compare two lobbys
extern int lobby_comparator (void *one, void *two);

// sets the lobby settings and a function to delete it
extern void lobby_set_game_settings (Lobby *lobby, void *game_settings, Action delete_game_settings);

// sets the lobby game data and a function to delete it
extern void lobby_set_game_data (Lobby *lobby, void *game_data, Action delete_lobby_game_data);

// set the lobby player handler
extern void lobby_set_handler (Lobby *lobby, Action handler);

// set lobby poll function timeout in mili secs
// how often we are checking for new packages
extern void lobby_set_poll_time_out (Lobby *lobby, unsigned int timeout);

// the default lobby id generator
extern void lobby_default_generate_id (char *lobby_id);

// searchs a lobby in the game data and returns a reference to it
extern Lobby *lobby_get (struct _GameServerData *game_data, Lobby *query);

/*** Player interaction ***/

// starts the lobby in a separte thread using its hanlder
extern u8 lobby_start (struct _Server *server, Lobby *lobby);

// creates a new lobby and inits his values with an owner
// pass a custom handler or NULL to use teh default one
extern Lobby *lobby_create (Server *server, Player *owner, unsigned int max_players, Action handler);

// called by a registered player that wants to join a lobby on progress
// the lobby model gets updated with new values
extern u8 lobby_join (struct _GameServerData *game_data, Lobby *lobby, struct _Player *player);

// called when a player requests to leave the lobby
extern u8 lobby_leave (struct _GameServerData *game_data, Lobby *lobby, struct _Player *player);

typedef struct ServerLobby {

    struct _Server *server;
    Lobby *lobby;

} ServerLobby;

#endif