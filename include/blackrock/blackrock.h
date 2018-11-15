#ifndef BLACKROCK_H_
#define BLACKROCK_H_

#include <stdint.h>

#define SCREEN_WIDTH    1280    
#define SCREEN_HEIGHT   720

#define MAP_WIDTH   80
#define MAP_HEIGHT  40

#define NUM_COLS    80
#define NUM_ROWS    45

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int32_t i32;
typedef int64_t i64;

typedef unsigned char asciiChar;

#define THREAD_OK   0

typedef struct Level {

    u8 levelNum;
    bool **mapCells;    // dungeon map

} Level;

// TODO: maybe game objects such as enemies with positions and items 
// go inside here?
typedef struct World {

	Level *level;

} World;

extern u8 blackrock_start_arcade (void *data);
extern u8 blackrock_loadGameData (void);

/*** BLACKROCK SERIALIZED DATA ***/

#endif