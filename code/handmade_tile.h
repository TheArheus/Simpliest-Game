#if !defined (HANDMADE_TILE_H)

struct tile_map_difference
{
    v2 dXY;
    real32 dZ;
};

struct tile_map_position 
{
    // There are fixed point tile location.
    // The high bits are the tile chink index, and low bits
    // are the tile index in the chunk
    uint32 AbsTileX;
    uint32 AbsTileY;
    uint32 AbsTileZ;

    v2 Offset;
};

struct tile_chunk_position
{
    uint32 TileChunkX;
    uint32 TileChunkY;
    uint32 TileChunkZ;

    uint32 RelTileX;
    uint32 RelTileY;
};

struct tile_chunk
{
    uint32* Tiles;
};

struct tile_map
{
    uint32 ChunkShift;
    uint32 ChunkMask;
    int32 ChunkDim;
 
    real32 TileSideInMeters;

    uint32 TileChunkCountX;
    uint32 TileChunkCountY;
    uint32 TileChunkCountZ;
    
    tile_chunk* TileChunks;
};

#define HANDMADE_TILE_H
#endif
