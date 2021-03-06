#include "handmade.h"
#include "handmade_tile.cpp"


static void GameOutputSound(game_sound_buffer* SoundBuffer, game_state* GameState, int ToneHz) {
	int16 ToneVolume = 3000;
	int WavePeriod = SoundBuffer->SamplesPerSeconds / ToneHz;

	int16* SampleOut = SoundBuffer->Samples;
	for (int SamplesIndex = 0; SamplesIndex < SoundBuffer->SamplesCount; ++SamplesIndex) {
#if 0
		real32 SineValue = sinf(GameState->tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
        int16 SampleValue = 0;
#endif
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;

#if 0
		GameState->tSine += (2.0f * Pi32) * 1.0f / (real32)WavePeriod;
		if(GameState->tSine > 2.0f * Pi32)
		{
			GameState->tSine -= 2.0f*Pi32;
		}
#endif
	}
}


internal void
DrawRectangle(game_offscreen_buf* BackBuffer, 
              v2 vMin, v2 vMax, 
              real32 R, real32 G, real32 B)
{
    int32 MinX = RoundReal32ToInt32(vMin.X);
    int32 MinY = RoundReal32ToInt32(vMin.Y);
    int32 MaxX = RoundReal32ToInt32(vMax.X);
    int32 MaxY = RoundReal32ToInt32(vMax.Y);

    if(MinX < 0) MinX = 0;
    if(MinY < 0) MinY = 0;
    if(MaxX > BackBuffer->Width) MaxX = BackBuffer->Width;
    if(MaxY > BackBuffer->Height) MaxY = BackBuffer->Height;


    uint8* Row = ((uint8*)BackBuffer->Memory + MinX*BackBuffer->BytesPerPixel + MinY*BackBuffer->Pitch);

    uint32 Color = ((RoundReal32ToUInt32(R * 255.0f) << 16) | 
                    (RoundReal32ToUInt32(G * 255.0f) << 8)  | 
                    (RoundReal32ToUInt32(B * 255.0f)));
    for(int Y = MinY; Y < MaxY; ++Y)
    {
        uint32* Pixel = (uint32*)Row;
		for (int X = MinX; X < MaxX; ++X)
        {
            *Pixel++ = Color;
		} 
        Row += BackBuffer->Pitch;
    }	
}

#pragma pack(push, 1)
struct bitmap_header
{
    uint16 FileType;
    uint32 FileSize;
    uint16 Reserved1;
    uint16 Reserved2;
    uint32 BitmapOffset;
    uint32 Size;
    int32 Width;
    int32 Height;
    uint16 Planes;
    uint16 BitsPerPixel;
    uint32 Compression;
    uint32 SizeOfBitmap;
    int32 HorzResolution;
    int32 VertResolution;
    uint32 ColorUsed;
    uint32 ColorImportant;

    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;
};
#pragma pack(pop)



internal loaded_bitmap
DEBUGLoadBMP(thread_context* Thread, debug_platform_read_entire_file* ReadEntireFile, char* FileName)
{
    loaded_bitmap Result = {};
    debug_read_file_result ReadResult = ReadEntireFile(Thread, FileName);

    if(ReadResult.ContentSize != 0)
    {
        // Pixel Format: AA BB GG RR
        // Little Endian: RR GG BB AA
        bitmap_header* Header = (bitmap_header*)ReadResult.Content;
        uint32* Pixels = (uint32*)((uint8*)ReadResult.Content + Header->BitmapOffset);
        Result.Pixels = Pixels;
        Result.Height = Header->Height;
        Result.Width = Header->Width;

        uint32 RedMask = Header->RedMask;
        uint32 GreenMask = Header->GreenMask;
        uint32 BlueMask = Header->BlueMask;
        uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

        bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
        bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);
 
        Assert(RedScan.Found);
        Assert(GreenScan.Found);
        Assert(BlueScan.Found);
        Assert(AlphaScan.Found);

        int32 RedShift   = 16 - (int32)RedScan.Index;
        int32 GreenShift = 8  - (int32)GreenScan.Index;
        int32 BlueShift  = 0  - (int32)BlueScan.Index;
        int32 AlphaShift = 24 - (int32)AlphaScan.Index;

        uint32* SourceDest = Pixels;
        for(int32 Y = 0; Y < Header->Height; ++Y)
        {
            for(int32 X = 0; X < Header->Width; ++X)
            {
                uint32 C = *SourceDest;

#if 0
                *SourceDest++ = ((((C >> AlphaScan.Index) & 0xFF) << 24) | 
                                 (((C >> RedScan.Index) & 0xFF) << 16)   |
                                 (((C >> GreenScan.Index) & 0xFF) << 8)  | 
                                 (((C >> BlueScan.Index) & 0xFF) << 0));
#else
                *SourceDest++ = (RotateLeft(C & RedMask, RedShift))    | 
                                (RotateLeft(C & GreenMask, GreenShift))|
                                (RotateLeft(C & BlueMask, BlueShift))  | 
                                (RotateLeft(C & AlphaMask, AlphaShift));

#endif
            }
        }
    }
    return Result;
}


internal void
DrawBitMap(game_offscreen_buf* BackBuffer, loaded_bitmap* BMP, real32 RealX, real32 RealY, int32 AlignX = 0, int32 AlignY = 0)
{
    RealX -= (real32)AlignX;
    RealY -= (real32)AlignY;

    int32 MinX = RoundReal32ToInt32(RealX);
    int32 MinY = RoundReal32ToInt32(RealY);
    int32 MaxX = RoundReal32ToInt32(RealX + (real32)BMP->Width);
    int32 MaxY = RoundReal32ToInt32(RealY + (real32)BMP->Height);

    int32 SourceOffsetX = 0;
    if(MinX < 0) {SourceOffsetX = -MinX; MinX = 0;}
    int32 SourceOffsetY = 0;
    if(MinY < 0) {SourceOffsetY = -MinY; MinY = 0;}

    if(MaxX > BackBuffer->Width) MaxX = BackBuffer->Width;
    if(MaxY > BackBuffer->Height) MaxY = BackBuffer->Height; 

    uint32* SourceRow = BMP->Pixels + BMP->Width*(BMP->Height - 1);
    SourceRow += -BMP->Width*SourceOffsetY + SourceOffsetX;

    uint8* DestRow = ((uint8*)BackBuffer->Memory + 
                       MinX*BackBuffer->BytesPerPixel +
                       MinY*BackBuffer->Pitch);
    for(int32 Y = MinY; Y < MaxY; ++Y)
    {
        uint32* Dest = (uint32*)DestRow;
        uint32* Source = SourceRow;
        for(int32 X = MinX; X < MaxX; ++X)
        {
            real32 A = (real32)((*Source >> 24) & 0xFF) / 255;

            real32 SR = (real32)((*Source >> 16) & 0xFF);
            real32 SG = (real32)((*Source >> 8)  & 0xFF);
            real32 SB = (real32)((*Source >> 0)  & 0xFF);

            real32 DR = (real32)((*Dest >> 16) & 0xFF);
            real32 DG = (real32)((*Dest >> 8)  & 0xFF);
            real32 DB = (real32)((*Dest >> 0)  & 0xFF);

            real32 R = (1.0f - A) * DR + A * SR;
            real32 G = (1.0f - A) * DG + A * SG;
            real32 B = (1.0f - A) * DB + A * SB;

            *Dest = (((uint32)(R + 0.5f) << 16) | 
                     ((uint32)(G + 0.5f) << 8)  | 
                     ((uint32)(B + 0.5f) << 0));

            ++Dest;
            ++Source;
        }

        DestRow += BackBuffer->Pitch;
        SourceRow -= BMP->Width;
    }
}

internal void
MovePlayer(game_state* GameState, entity* Entity, real32 dt, v2 ddPlayer)
{
    tile_map* TileMap = GameState->World->TileMap;
    
    real32 ddLength = LengthSq(ddPlayer);
    if(ddLength > 1.0f)
    {
        ddPlayer /= SquareRoot(ddLength);
    } 

    real32 PlayerSpeed = 30.0f; // m/s^2
    ddPlayer *= PlayerSpeed; 

    // ODE Here
    ddPlayer += -8.0f*Entity->dP; 

    tile_map_position OldPlayerP = Entity->P;
    tile_map_position NewPlayerP = OldPlayerP;
    v2 PlayerDelta = (0.5f*ddPlayer*Square(dt) + 
                      dt*Entity->dP);
    NewPlayerP.Offset += PlayerDelta;
    Entity->dP = ddPlayer * dt + Entity->dP;

    NewPlayerP = RecanonicalizePos(TileMap, NewPlayerP);

#if 1
    tile_map_position PlayerLeftPos = NewPlayerP;
    PlayerLeftPos.Offset.X -= 0.5f * Entity->Width;
    PlayerLeftPos = RecanonicalizePos(TileMap, PlayerLeftPos);

    tile_map_position PlayerRightPos = NewPlayerP;
    PlayerRightPos.Offset.X += 0.5f * Entity->Width;
    PlayerRightPos = RecanonicalizePos(TileMap, PlayerRightPos);

    bool32 Collided = false;
    tile_map_position ColP = {};
    if(!IsTileMapEmpty(TileMap, NewPlayerP))
    {
        ColP = NewPlayerP;
        Collided = true;
    }
    if(!IsTileMapEmpty(TileMap, PlayerLeftPos))
    {
        ColP = PlayerLeftPos;
        Collided = true;
    }
    if(!IsTileMapEmpty(TileMap, PlayerRightPos))
    {
        ColP = PlayerRightPos;
        Collided = true;
    }

    if(Collided)
    {
        v2 r = {0, 0};

        if(ColP.AbsTileX < Entity->P.AbsTileX)
        {
            r = v2{1, 0};
        }
        if(ColP.AbsTileX > Entity->P.AbsTileX)
        {
            r = v2{-1, 0};
        }
        if(ColP.AbsTileY < Entity->P.AbsTileY)
        {
            r = v2{0, 1};
        }
        if(ColP.AbsTileY > Entity->P.AbsTileY)
        {
            r = v2{0, -1};
        }

        Entity->dP = Entity->dP - Inner(Entity->dP, r)*r;
    }
    else
    {
        Entity->P = NewPlayerP;
    }
#else
    uint32 MinTileX = 0;
    uint32 MinTileY = 0;
    uint32 OnePastMaxTileX = 0;
    uint32 OnePastMaxTileY = 0;
    uint32 AbsTileZ = GameState->PlayerP.AbsTileZ;
    tile_map_position BestPlayerP = GameState->PlayerP;
    real32 BestDistance = LengthSq(PlayerDelta);
    for(uint32 AbsTileX = MinTileX; AbsTileX != OnePastMaxTileX; ++AbsTileX)
    {
        for(uint32 AbsTileY = MinTileY; AbsTileY != OnePastMaxTileY; ++AbsTileY)
        {
            tile_map_position TestTileP = CenteredTilePoint(AbsTileX, AbsTileY, AbsTileZ);
            uint32 TileValue = GetTileValue(TileMap, TestTileP);

            if(IsTileMapEmpty(TileValue))
            {
                v2 MinCorner = -0.5f * v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};
                v2 MaxCorner = 0.5f * v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};

                tile_map_difference RelNewPlayerP = Substract(TileMap, &TestTileP, &NewPlayerP);
                v2 TestP = ClosestPointInRectangle(MinCorner, MaxCorner, RelNewPlayerP);
                if()
                {
                    
                }
            }
        }
    }
#endif

    if(!AreOnSameTile(&OldPlayerP, &Entity->P))
    {
        uint32 NewTileValue = GetTileValue(TileMap, Entity->P);
        if(NewTileValue == 3)
        {
            ++Entity->P.AbsTileZ;
        }
        else if(NewTileValue == 4)
        {
            --Entity->P.AbsTileZ;
        }
    }
    
    if((Entity->dP.X == 0.0f) && (Entity->dP.Y == 0.0f))
    {
    }
    else if(AbsoluteValue(Entity->dP.X) > AbsoluteValue(Entity->dP.Y))
    {
        if(Entity->dP.X > 0)
        {
            Entity->FacingDirection = 0;
        }
        else
        {
            Entity->FacingDirection = 2;
        }
    }
    else if(AbsoluteValue(Entity->dP.X) < AbsoluteValue(Entity->dP.Y))
    {
        if(Entity->dP.Y > 0)
        {
            Entity->FacingDirection = 1;
        }
        else
        {
            Entity->FacingDirection = 3;
        }
    }
}

internal uint32
AddEntity(game_state* GameState)
{
    uint32 EntityIndex = GameState->EntityCount++;
    Assert(GameState->EntityCount < ArrayCount(GameState->Entities))
    entity* Entity = &GameState->Entities[EntityIndex];

    *Entity = {};
    
    return EntityIndex;
}

inline entity*
GetEntity(game_state* GameState, uint32 Index)
{
    entity* Entity = 0;
    
    if((Index > 0) && (Index < ArrayCount(GameState->Entities)))
    {
        Entity = &GameState->Entities[Index];
    }

    return Entity;
}


internal void
InitializePlayer(game_state* GameState, uint32 EntityIndex)
{
    entity* Entity = GetEntity(GameState, EntityIndex);

    Entity->Exists = true;
    Entity->P.AbsTileX = 1;
    Entity->P.AbsTileY = 3;
    Entity->P.Offset.X = 5.0f;
    Entity->P.Offset.Y = 5.0f;
    Entity->Width = 1.4f;
    Entity->Height = 0.75f*Entity->Height;

    if(!GetEntity(GameState, GameState->CameraFollowingEntityIndex))
    {
        GameState->CameraFollowingEntityIndex = EntityIndex;
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) 
{
	Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

#define TILE_MAP_COUNT_X 256
#define TILE_MAP_COUNT_Y 256

    real32 PlayerHeight      = (real32)1.4f;
    real32 PlayerWidth       = (real32)PlayerHeight * 0.75f;
    real32 MetersToPixels;

    game_state* GameState = (game_state*)Memory->PermanentStorage;
	if (!Memory->IsInitialized) 
	{
        uint32 NullEntityIndex = AddEntity(GameState);

		Memory->IsInitialized = true; 

        GameState->Backprop = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");

        player_bitmaps* Bitmap;

        Bitmap = GameState->PlayerBitmaps;
        Bitmap->PlayerHead  = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_head.bmp");
        Bitmap->PlayerCape  = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_cape.bmp");
        Bitmap->PlayerTorso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;
        ++Bitmap;

        Bitmap->PlayerHead  = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_head.bmp");
        Bitmap->PlayerCape  = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_cape.bmp");
        Bitmap->PlayerTorso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;
        ++Bitmap;
        
        Bitmap->PlayerHead  = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_head.bmp");
        Bitmap->PlayerCape  = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_cape.bmp");
        Bitmap->PlayerTorso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;
        ++Bitmap;

        Bitmap->PlayerHead  = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp");
        Bitmap->PlayerCape  = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
        Bitmap->PlayerTorso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_torso.bmp");
        Bitmap->AlignX = 72;
        Bitmap->AlignY = 182;
        ++Bitmap;

        GameState->CameraP.AbsTileX = 17 / 2;
        GameState->CameraP.AbsTileY = 8 / 2;

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), (uint8*)Memory->PermanentStorage + sizeof(game_state));

        tile_map* TileMap;

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world* World     = GameState->World;
        World->TileMap   = PushStruct(&GameState->WorldArena, tile_map);

        TileMap = World->TileMap;
        
        // Fow 256x256 chunks
        TileMap->ChunkShift = 4;
        TileMap->ChunkMask  = (1 << TileMap->ChunkShift) - 1;
        TileMap->ChunkDim   = (1 << TileMap->ChunkShift);

        TileMap->TileChunkCountX   = 128;
        TileMap->TileChunkCountY   = 128;
        TileMap->TileChunkCountZ   = 2;
        TileMap->TileChunks        = PushArray(&GameState->WorldArena, TileMap->TileChunkCountX * TileMap->TileChunkCountY * TileMap->TileChunkCountZ, tile_chunk);

        TileMap->TileSideInMeters  = 1.4f;
        
        uint32 TilesPerWidth  = 17;
        uint32 TilesPerHeight = 9;

        uint32 ScreenX = 0;
        uint32 ScreenY = 0;

        uint32 RandomNumberIndex = 0;
        uint32 AbsTileZ          = 0;

        bool32 DoorLeft   = false;
        bool32 DoorRight  = false;
        bool32 DoorTop    = false;
        bool32 DoorBottom = false;
        bool32 DoorUp     = false;
        bool32 DoorDown   = false;

        for(uint32 ScreenIndex = 0; ScreenIndex < 100; ++ScreenIndex)
        {
            Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
            uint32 RandomChoice; 
            if(DoorUp || DoorDown)
            {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
            }
            else
            {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3; 
            }

            bool32 CreatedZDoor = false;

            if(RandomChoice == 2)
            {
                CreatedZDoor = true;
                if(AbsTileZ == 0)
                {
                    DoorUp = true;
                }
                else if(AbsTileZ == 1)
                {
                    DoorDown = true;
                }
            }
            else if(RandomChoice == 1)
            {
                DoorRight = true;
            }
            else 
            {
                DoorTop = true;
            }
            
            for(uint32 TileY = 0; TileY < TilesPerHeight; ++TileY)
            {
                for(uint32 TileX = 0; TileX < TilesPerWidth; ++TileX)
                {
                    uint32 AbsTileX = ScreenX*TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY*TilesPerHeight + TileY;
                   
                    uint32 TileValue = 1;
                    if((TileX == 0) && (!DoorLeft || (TileY != (TilesPerHeight/2))))
                    {
                        TileValue = 2;
                    }
                    if((TileX == (TilesPerWidth - 1)) && (!DoorRight || (TileY != (TilesPerHeight/2)))) 
                    {
                        TileValue = 2;
                    }
                    if((TileY == 0) && (!DoorBottom || (TileX != (TilesPerWidth/2))))
                    {
                        TileValue = 2;
                    } 
                    if((TileY == (TilesPerHeight - 1)) && (!DoorTop || (TileX != (TilesPerWidth/2))))    
                    {
                        TileValue = 2;
                    }

                    if((TileX == 10) && (TileY == 6))
                    {
                        if(DoorUp)
                        {
                            TileValue = 3;
                        }

                        if(DoorDown)
                        {
                            TileValue = 4;
                        }
                    }
                    SetTileValue(&GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY, AbsTileZ, TileValue);
                }
            } 
            
            DoorLeft = DoorRight;
            DoorBottom = DoorTop;

            if(CreatedZDoor)
            {
                DoorDown = !DoorDown;
                DoorUp   = !DoorUp;
            }
            else 
            {
                DoorDown = false;
                DoorUp   = false;
            }

            DoorRight = false;
            DoorTop   = false;

            if(RandomChoice == 2)
            {
                if(AbsTileZ == 0)
                {
                    AbsTileZ = 1;
                }
                else if(AbsTileZ == 1)
                {
                    AbsTileZ = 0;
                }
            }
            else if(RandomChoice == 1)
            {
                ScreenX += 1;
            }
            else 
            {
                ScreenY += 1;
            }

        }
    }


    world* World      = GameState->World;
    tile_map* TileMap = World->TileMap;
    
    int32 TileSideInPixels    = 60;
    TileMap->TileSideInMeters = 1.4f;
    MetersToPixels            = (real32)(TileSideInPixels / TileMap->TileSideInMeters);

    real32 LowerLeftX = -(real32)TileSideInPixels / 2;
    real32 LowerLeftY = (real32)BackBuffer->Height;
        

	
    for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex) 
    {
		game_controller_input* Controller = GetController(Input, ControllerIndex);
        entity* ControllingEntity = GetEntity(GameState, GameState->PlayerIndexForController[ControllerIndex]);
        if(ControllingEntity)
        {
            v2 ddPlayer = {};

            if (Controller->IsAnalog) 
            {	
                ddPlayer = v2{Controller->StickAvarageX, Controller->StickAvarageY};
            }
            else 
            {
                if(Controller->MoveUp.EndedDown){ddPlayer.Y = 1.0f;}
                if(Controller->MoveDown.EndedDown){ddPlayer.Y = -1.0f;}
                if(Controller->MoveLeft.EndedDown){ddPlayer.X = -1.0f;}
                if(Controller->MoveRight.EndedDown){ddPlayer.X = 1.0f;}
            }
            MovePlayer(GameState, ControllingEntity, Input->dtForFrame, ddPlayer);
        }
        else
        {
            if(Controller->Start.EndedDown)
            {
                uint32 EntityIndex = AddEntity(GameState);
                InitializePlayer(GameState, EntityIndex);
                GameState->PlayerIndexForController[ControllerIndex] = EntityIndex;
            }
        }
    }


    entity* CameraFollowingEntity = GetEntity(GameState, GameState->CameraFollowingEntityIndex);
    if(CameraFollowingEntity)
    {
        GameState->CameraP.AbsTileZ = CameraFollowingEntity->P.AbsTileZ;
    
        tile_map_difference Diff = Substract(TileMap, &CameraFollowingEntity->P, &GameState->CameraP); 
        if(Diff.dXY.X > (9.0f*TileMap->TileSideInMeters))
        {
            GameState->CameraP.AbsTileX += 17;
        }
        if(Diff.dXY.X < -(9.0f*TileMap->TileSideInMeters))
        {
            GameState->CameraP.AbsTileX -= 17;
        }
        if(Diff.dXY.Y > (5.0f*TileMap->TileSideInMeters))
        {
            GameState->CameraP.AbsTileY += 9;
        }
        if(Diff.dXY.Y < -(5.0f*TileMap->TileSideInMeters))
        {
            GameState->CameraP.AbsTileY -= 9;
        }

    }
    DrawBitMap(BackBuffer, &GameState->Backprop, 0, 0);
    
    real32 ScreenCenterX = BackBuffer->Width * 0.5f;
    real32 ScreenCenterY = BackBuffer->Height * 0.5f;

    for(int32 RelRow = -10; RelRow < 10; ++RelRow)
    {
        for(int32 RelColumn = -20; RelColumn < 20; ++RelColumn)
        {
            uint32 Column = RelColumn + GameState->CameraP.AbsTileX;
            uint32 Row    = RelRow + GameState->CameraP.AbsTileY;

            uint32 TileID = GetTileValue(TileMap, Column, Row, GameState->CameraP.AbsTileZ);
            if(TileID > 1){
                real32 Gray = 0.5f;

                if(TileID == 2) Gray = 1.0f;
                if(TileID > 2) Gray = 0.25f;
                if((Row == GameState->CameraP.AbsTileY) && (Column == GameState->CameraP.AbsTileX)) Gray = 0.0f;

                v2 TileSide = {0.5f*TileSideInPixels, 0.5f*TileSideInPixels};
                v2 Cent = {ScreenCenterX - MetersToPixels*GameState->CameraP.Offset.X + ((real32)RelColumn) * TileSideInPixels,
                           ScreenCenterY + MetersToPixels*GameState->CameraP.Offset.Y- ((real32)RelRow) * TileSideInPixels};
                
                v2 Min = Cent - TileSide;
                v2 Max = Cent + TileSide;

                DrawRectangle(BackBuffer, Min, Max, Gray, Gray, Gray);
            }
        }
    }

    entity* Entity = GameState->Entities;
    for(uint32 EntityIndex = 0; EntityIndex < GameState->EntityCount; ++EntityIndex, ++Entity)
    {
        if(Entity->Exists)
        {
            tile_map_difference Diff = Substract(TileMap, &Entity->P, &GameState->CameraP);
            
            real32 PlayerR = 0.5f;
            real32 PlayerG = 0.5f;
            real32 PlayerB = 1.0f;

            real32 PlayerGroundPointX = ScreenCenterX + MetersToPixels*Diff.dXY.X;
            real32 PlayerGroundPointY = ScreenCenterY - MetersToPixels*Diff.dXY.Y;
            
            player_bitmaps* PlayerBitmap = &GameState->PlayerBitmaps[Entity->FacingDirection];
            DrawBitMap(BackBuffer, &PlayerBitmap->PlayerTorso, PlayerGroundPointX, PlayerGroundPointY, PlayerBitmap->AlignX, PlayerBitmap->AlignY);
            DrawBitMap(BackBuffer, &PlayerBitmap->PlayerCape, PlayerGroundPointX, PlayerGroundPointY, PlayerBitmap->AlignX, PlayerBitmap->AlignY);
            DrawBitMap(BackBuffer, &PlayerBitmap->PlayerHead, PlayerGroundPointX, PlayerGroundPointY, PlayerBitmap->AlignX, PlayerBitmap->AlignY);
        }
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state* GameState = (game_state*)Memory->PermanentStorage;
}
