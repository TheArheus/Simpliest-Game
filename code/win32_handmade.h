#if !defined(WIN32_HANDMADE_H)
struct win32_offscreen_buf {
	BITMAPINFO	BitMapInfo;
	void* Memory;
	int			Width;
	int			Height;
	int			Pitch;
	int			BytesPerPixel;
};

struct win32_sound_output {
	int		SamplesPerSeconds;
	int		ToneHz;
	int16	ToneVolume;
	uint32	RunningSampleIndex;
	int		WavePeriod;
	int		BytesPerSample;
	int		SecondaryBufferSize;
	int		LatencySampleCount;
	int		SafetyBytes;
};

struct win32_debug_time_marker {
	DWORD OutputPlayCursor;
	DWORD OutputWriteCursor;
	DWORD OutputLocation;
	DWORD OutputByteCount;
	DWORD ExpectedFlipPlayCursor;

	DWORD FlipPlayCursor;
	DWORD FlipWriteCursor;
};

struct win32_game_code
{
    HMODULE GameCodeLibrary; 
    FILETIME LastWriteTime;
    game_update_and_render* UpdateAndRender;
    game_get_sound_samples* GetSoundSamples;
    bool32 IsValid;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_replay_buffer
{
    HANDLE Handle;
    HANDLE MemoryMap;
    char ReplayFileName[WIN32_STATE_FILE_NAME_COUNT];
    void* MemoryBlock;
};
struct win32_state
{
    uint64 TotalSize;
    void* GameMemoryBlock;
    win32_replay_buffer ReplayBuffers[4];

    HANDLE RecordingHandle;
    int InputRecordingIndex;

    HANDLE PlaybackHandle;
    int InputPlayingIndex; 


    char EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
    char* OnePastLastEXEFileNameSlash;
};
#define WIN32_HANDMADE_H
#endif
