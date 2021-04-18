#if !defined(HANDMADE_PLATFORM_H)
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
/*
	NOTE: Services that game provide to the platform layer
*/
// Things to use - Timing, controller/keyboard input, bitmap buffer to use, sound buffer to use
#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
#undef COMPILER_LLVM
#define COMPILER_LLVM
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#endif

#define internal		static 
#define global_variable static 
#define local_persist	static

typedef int8_t		int8;
typedef int16_t		int16;
typedef int32_t		int32;
typedef int64_t		int64;

typedef uint8_t		uint8;
typedef uint16_t	uint16;
typedef uint32_t	uint32;
typedef uint64_t	uint64;

typedef size_t      mem_index;

typedef float		real32;
typedef double		real64;

typedef int32_t		bool32;

#if _DEBUG
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

typedef struct thread_context
{
    int Placeholder;
} thread_context;

#if HANDMADE_INTERNAL
typedef struct debug_read_file_result {
	uint32 ContentSize;
	void* Content;
} debug_read_file_result;

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context* Thread, void* Memory);
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context* Thread, const char* Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);
#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(thread_context* Thread, const char* Filename, uint32 MemorySize, void* Memory);
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#endif


typedef struct game_offscreen_buf {
	void*		Memory;
	int			Width;
	int			Height;
	int			Pitch;
    int         BytesPerPixel;
} game_offscreen_buf;

typedef struct game_sound_buffer {
	int		SamplesPerSeconds;
	int		SamplesCount;
	int16*	Samples;
} game_sound_buffer;

typedef struct game_button_state {
	int 	HalfTransitionCount;
	bool32 	EndedDown;
} game_button_state;

typedef struct game_controller_input {
	bool32 IsAnalog;
	bool32 IsConnected;
	
	real32 StickAvarageX;
	real32 StickAvarageY;

	union {
		game_button_state Buttons[12];
		struct {
			game_button_state MoveUp;
			game_button_state MoveDown;
			game_button_state MoveLeft;
			game_button_state MoveRight;

			game_button_state ActionUp;
			game_button_state ActionDown;
			game_button_state ActionLeft;
			game_button_state ActionRight;

			game_button_state LeftShoulder;
			game_button_state RightShoulder;

			game_button_state Start;
			game_button_state Back;

			game_button_state Terminator;
		};
	};
} game_controller_input;

typedef struct game_input {
    game_button_state MouseButtons[5];
    int32 MouseX, MouseY, MouseZ;

    real32 dtForFrame; 
	game_controller_input Controllers[5];
} game_input;

typedef struct game_memory {
	bool32 IsInitialized;
	
	uint64 PermanentStorageSize;
	void* PermanentStorage;
	
	uint64 TransientStorageSize;
	void* TransientStorage;

    debug_platform_read_entire_file* DEBUGPlatformReadEntireFile;
    debug_platform_free_file_memory* DEBUGPlatformFreeFileMemory;
    debug_platform_write_entire_file* DEBUGPlatformWriteEntireFile;
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) void name(thread_context* Thread, game_memory* Memory, game_input* Input, game_offscreen_buf* BackBuffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context* Thread,game_memory* Memory, game_sound_buffer* SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#ifdef __cplusplus
}
#endif

#define HANDMADE_PLATFORM_H
#endif
