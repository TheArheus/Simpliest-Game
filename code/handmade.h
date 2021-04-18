#if !defined(HANDMADE_H)

#include "handmade_platform.h"
#include "handmade_random.h"

#define KiloBytes_(value) ((value) * 1024LL)
#define MegaBytes_(value) (KiloBytes_(value) * 1024LL)
#define GigaBytes_(value) (MegaBytes_(value) * 1024LL)
#define TeraBytes_(value) (GigaBytes_(value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Pi32 3.141592653589793238f

inline uint32 SafeTruncateUInt64(uint64 Value) {
	Assert(Value <= 0xFFFFFFFF);
	uint32 Result = (uint32)Value;
	return Result;
}

inline game_controller_input* GetController(game_input* Input, unsigned int ControllerIndex) {
	Assert(ControllerIndex < ArrayCount(Input->Controllers));

	game_controller_input* Result = &Input->Controllers[ControllerIndex];
	return Result;
}

struct memory_arena
{
    mem_index Size;
    uint8* Base;
    mem_index Used;
};

internal void
InitializeArena(memory_arena* Arena, mem_index Size, uint8* Base)
{
    Arena->Size = Size;
    Arena->Base = Base;
    Arena->Used = 0;
}

void* 
PushSize_(memory_arena* Arena, mem_index Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void* Result = Arena->Base + Arena->Used;
    Arena->Used += Size;

    return Result;
}

#define PushStruct(Arena, type) (type*)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type*)PushSize_(Arena, Count*sizeof(type))


#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_tile.h"

struct world
{
    tile_map* TileMap;
};

struct loaded_bitmap
{
    int32 Width;
    int32 Height;
    uint32* Pixels;
};

struct player_bitmaps
{
    int32 AlignX;
    int32 AlignY;

    loaded_bitmap PlayerHead;
    loaded_bitmap PlayerCape;
    loaded_bitmap PlayerTorso;
};

struct entity
{
    bool32 Exists;
    tile_map_position P;
    v2 dP;
    uint32 FacingDirection;
    real32 Width, Height;
};

struct game_state 
{ 
    world* World;
    memory_arena WorldArena;

    uint32 CameraFollowingEntityIndex;
    tile_map_position CameraP;

    uint32 PlayerIndexForController[ArrayCount(((game_input*)0)->Controllers)];
    uint32 EntityCount;
    entity Entities[256];

    loaded_bitmap Backprop; 
    uint32 PlayerFacingDirection;
    player_bitmaps PlayerBitmaps[4];
}; 

#define HANDMADE_H
#endif
