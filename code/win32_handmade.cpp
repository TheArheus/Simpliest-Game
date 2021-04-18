#include "handmade.h"

#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <malloc.h>

#include "win32_handmade.h"


struct win32_window_dimensions {
	int Width;
	int Height;
};


global_variable bool32				GlobalPause;
global_variable bool32				GlobalRunning;
global_variable win32_offscreen_buf GlobalBackBuffer;
global_variable IDirectSoundBuffer* GlobalSecondarySBuffer;
global_variable int64 				GlobalPerfCountFrequency;
global_variable bool32              DEBUGGlobalShowCursor;
global_variable WINDOWPLACEMENT     GlobalWindowPosition = {sizeof(GlobalWindowPosition)};



// NOTE: Support for XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE: Support for XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_


#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);


void DEBUGPlatformFreeFileMemory(thread_context* Thread, void* Memory)
{
	if (Memory) {
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

debug_read_file_result DEBUGPlatformReadEntireFile(thread_context* Thread, const char* Filename)
{
	debug_read_file_result Result = {};

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE) 
	{
		LARGE_INTEGER FileSize;
		if (GetFileSizeEx(FileHandle, &FileSize)) 
		{
			uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
			Result.Content = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			if (Result.Content) 
			{
				DWORD BytesRead;
				if (ReadFile(FileHandle, Result.Content, FileSize32, &BytesRead, 0) && 
					(FileSize32 == BytesRead)) 
				{
					Result.ContentSize = FileSize32;
				}
				else 
				{
					DEBUGPlatformFreeFileMemory(Thread, Result.Content);
					Result.Content = 0;
				}
			}
			else 
			{
				//Logging
			}
		}
		else 
		{
			//Logging
		}
		CloseHandle(FileHandle);
	}
	
	return Result;
}

bool32 DEBUGPlatformWriteEntireFile(thread_context* Thread, const char* Filename, uint32 MemorySize, void* Memory)
{
	bool32 Result = false;

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;
		if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
		{
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			// Logging
		}

		CloseHandle(FileHandle);
	}
	else
	{
		// Logging
	}
	return bool32();
}

internal int
StringLength(char* String)
{
    int Length = 0;
    while(*String++)
    {
        ++Length;
    }
    return Length;
}

void
CatStrings(size_t SourceACount, char* SourceA, 
           size_t SourceBCount, char* SourceB, 
           size_t DestCount, char* Dest)
{
    for(int Index = 0; Index < SourceACount; ++Index)
    {
        *Dest++ = *SourceA++;
    }
    for(int Index = 0; Index < SourceBCount; ++Index)
    {
        *Dest++ = *SourceB++;
    }
    *Dest++ = 0;
}

internal void
Win32BuildEXEPathFileName(win32_state* State, char* FileName, int DestCount, char* Dest)
{
    CatStrings(State->OnePastLastEXEFileNameSlash - State->EXEFileName, State->EXEFileName, StringLength(FileName), FileName, DestCount, Dest);
}

void Win32LoadXInput(void) {
	HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");
	if (!XInputLibrary) 
    {
		//TODO: Diagnostic
		XInputLibrary = LoadLibrary("xinput1_3.dll");
	}

	if (XInputLibrary) 
    {
		XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");

		//TODO: Diagnostic
	}
	else 
    {
		//TODO: Diagnostic
	}
}

inline FILETIME
Win32GetLastWriteTime(char* FileName)
{
    FILETIME LastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA Data;
    if(GetFileAttributesExA(FileName, GetFileExInfoStandard, &Data))
    {
        LastWriteTime = Data.ftLastWriteTime;
    }
    return LastWriteTime;
}

internal win32_game_code 
Win32LoadGameCode(char* SourceDLLName, char* TempDLLName, char* LockedFileName)
{ 
    win32_game_code Result = {}; 
    
    WIN32_FILE_ATTRIBUTE_DATA Ignored;
    if(!GetFileAttributesEx(LockedFileName, GetFileExInfoStandard, &Ignored)){
        Result.LastWriteTime = Win32GetLastWriteTime(SourceDLLName);

        CopyFile(SourceDLLName, TempDLLName, FALSE);

        Result.GameCodeLibrary = LoadLibraryA(TempDLLName);
        if(Result.GameCodeLibrary)
        {
            Result.UpdateAndRender = (game_update_and_render*)GetProcAddress(Result.GameCodeLibrary, "GameUpdateAndRender");
            Result.GetSoundSamples = (game_get_sound_samples*)GetProcAddress(Result.GameCodeLibrary, "GameGetSoundSamples");

            Result.IsValid = (Result.UpdateAndRender &&
                              Result.GetSoundSamples);
        }
    }
    if(!Result.IsValid)
    {
        Result.GetSoundSamples = 0;
        Result.UpdateAndRender = 0;
    }
    return Result;
}

internal void Win32UnloadGameCode(win32_game_code* GameCode)
{
    if(GameCode->GameCodeLibrary)
    {
        FreeLibrary(GameCode->GameCodeLibrary);
        GameCode->GameCodeLibrary = 0;
    }

    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

internal void Win32InitDSound(HWND Window, int32 SamplesPerSec, int32 BufferSize) 
{
	//NOTE: Load Library
	HMODULE DSoundLibrary = LoadLibrary("dsound.dll");

	if (DSoundLibrary) 
    {
		//NOTE: Get a DirectSound object
		direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		IDirectSound* DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
			WAVEFORMATEX WaveFormat = {};

			WaveFormat.wFormatTag		= WAVE_FORMAT_PCM;
			WaveFormat.nChannels		= 2;
			WaveFormat.nSamplesPerSec	= SamplesPerSec;
			WaveFormat.wBitsPerSample	= 16;
			WaveFormat.nBlockAlign		= (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec	= WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
			WaveFormat.cbSize			= 8;

			//NOTE: "Create" a primary buffer
			HRESULT result = DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY);
			if (SUCCEEDED(result)) {
				IDirectSoundBuffer* PrimarySBuffer;
				DSBUFFERDESC BufferDescription = {};

				BufferDescription.dwSize	= sizeof(BufferDescription);
				BufferDescription.dwFlags	= DSBCAPS_PRIMARYBUFFER;

				result = DirectSound->CreateSoundBuffer(&BufferDescription, &PrimarySBuffer, 0);
				if (SUCCEEDED(result)) {
					result = PrimarySBuffer->SetFormat(&WaveFormat);
					if (SUCCEEDED(result)) {
						//NOTE: Set the format

					}
					else {
						//TODO: Diagnostic
					}
				}
			}
			else {
				//TODO: Diagnostic
			}
			//NOTE: "Create" a secondary buffer
			DSBUFFERDESC BufferDescription = {};

			BufferDescription.dwSize		= sizeof(BufferDescription);
			BufferDescription.dwFlags		= DSBCAPS_GETCURRENTPOSITION2;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat	= &WaveFormat;

			result = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondarySBuffer, 0);
			if (SUCCEEDED(result)) {
				result = GlobalSecondarySBuffer->SetFormat(&WaveFormat);
				if (SUCCEEDED(result)) {
					//NOTE: Start it playing
				}
			}
		}
		else {
			//TODO: Diagnostic
		}
	}
	else {
		//TODO: Diagnostic
	}
}

internal win32_window_dimensions 
Win32GetWindowDimensions(HWND Window) {
	win32_window_dimensions Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width	= ClientRect.right	- ClientRect.left;
	Result.Height	= ClientRect.bottom - ClientRect.top;

	return Result;
}

void Win32ClearBuffer(win32_sound_output* SoundOutput) {
	void* Region1;
	DWORD Region1Size;
	void* Region2;
	DWORD Region2Size;

	if (SUCCEEDED(GlobalSecondarySBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
		&Region1, &Region1Size,
		&Region2, &Region2Size,
		0)))
	{
		int8* DestSample = (int8*)Region1;
		for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex) {
			*DestSample++ = 0;
		}
		DestSample = (int8*)Region2;
		for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex) {
			*DestSample++ = 0;
		}
	}
	GlobalSecondarySBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
}

void Win32FillSoundBuffer(win32_sound_output* SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_buffer* SourceBuffer) {
	void* Region1;
	DWORD Region1Size;
	void* Region2;
	DWORD Region2Size;

	if (SUCCEEDED(GlobalSecondarySBuffer->Lock(ByteToLock, BytesToWrite,
		&Region1, &Region1Size,
		&Region2, &Region2Size,
		0)))
	{
		int16* DestSample = (int16*)Region1;
		int16* SourceSample = SourceBuffer->Samples;
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		for (DWORD SamplesIndex = 0; SamplesIndex < Region1SampleCount; ++SamplesIndex) {
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;

			++SoundOutput->RunningSampleIndex;
		}

		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		DestSample = (int16*)Region2;
		for (DWORD SamplesIndex = 0; SamplesIndex < Region2SampleCount; ++SamplesIndex) {
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;

			++SoundOutput->RunningSampleIndex;
		}
		GlobalSecondarySBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal void 
Win32ResizeDIBSection(win32_offscreen_buf* Buffer, int width, int height)
{
	if (Buffer->Memory) {
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width			= width;
	Buffer->Height			= height;
	Buffer->BytesPerPixel	= 4;

	Buffer->BitMapInfo.bmiHeader.biSize			= sizeof(Buffer->BitMapInfo.bmiHeader);
	Buffer->BitMapInfo.bmiHeader.biWidth		= Buffer->Width;
	Buffer->BitMapInfo.bmiHeader.biHeight		= -Buffer->Height;
	Buffer->BitMapInfo.bmiHeader.biPlanes		= 1;
	Buffer->BitMapInfo.bmiHeader.biBitCount		= 32;
	Buffer->BitMapInfo.bmiHeader.biCompression	= BI_RGB;

	int BitmapMemorySize = Buffer->BytesPerPixel * (Buffer->Width * Buffer->Height);

	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
}

internal void 
Win32DisplayBufferInWindow(win32_offscreen_buf* Buffer, HDC DeviceContest, int WindowWidth, int WindowHeight)
{
    if((WindowWidth >= Buffer->Width*2) && 
       (WindowHeight >= Buffer->Height*2))
    {
        StretchDIBits(DeviceContest, 
                      0, 0, 2 * WindowWidth, 2 * WindowHeight,
                      0, 0, Buffer->Width, Buffer->Height, 
                      Buffer->Memory, &Buffer->BitMapInfo, DIB_RGB_COLORS, SRCCOPY);
    }
    else
    {
        int OffsetX = 10;
        int OffsetY = 10;

        PatBlt(DeviceContest, 0, 0, WindowWidth, OffsetX, BLACKNESS);
        PatBlt(DeviceContest, 0, 0, OffsetX, WindowHeight, BLACKNESS);
        PatBlt(DeviceContest, 0, Buffer->Height + OffsetY, WindowWidth, WindowHeight, BLACKNESS);
        PatBlt(DeviceContest, Buffer->Width + OffsetX, 0, WindowWidth, WindowHeight, BLACKNESS);
        
        StretchDIBits(DeviceContest, 
            OffsetX, OffsetY, Buffer->Width, Buffer->Height, 
            0, 0, Buffer->Width, Buffer->Height,
            Buffer->Memory, &Buffer->BitMapInfo, DIB_RGB_COLORS, SRCCOPY);
    }
}

internal void Win32ProccesXInputButton(game_button_state* NewState, 
									   game_button_state* OldState, 
									   DWORD ButtonBit, DWORD XInputButtonState) 
{
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32 Win32ProcessXInputStickValue(SHORT ThumbValue, SHORT Deadzone) {
	real32 result = 0;

	if (ThumbValue < -Deadzone) {
		result = (real32)((ThumbValue + Deadzone) / (32768.0f - Deadzone));
	}
	else if (ThumbValue > Deadzone)
	{
		result = (real32)((ThumbValue - Deadzone) / (32767.0f + Deadzone));
	}
	return result;
}

internal void
Win32ProcessKeyboardMessage(game_button_state* NewState, bool32 IsDown)
{
	if(NewState->EndedDown != IsDown){
        NewState->EndedDown = IsDown;
	    ++NewState->HalfTransitionCount;
    }
}

// Recording Input 

internal void
Win32GetInputFileLocation(win32_state* State, bool32 InputStream, 
                          int SlotIndex, int DestCount, char* Dest)
{
    char Temp[64];
    wsprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
    Win32BuildEXEPathFileName(State, Temp, DestCount, Dest); 
}

internal win32_replay_buffer* Win32GetReplayBuffer(win32_state* State, int unsigned ReplayIndex)
{
    Assert(ReplayIndex < ArrayCount(State->ReplayBuffers));
    win32_replay_buffer* Result = &State->ReplayBuffers[ReplayIndex];
    return Result;
}

internal void
Win32BeginRecordingInput(win32_state* Win32State, int InputRecordingIndex)
{
    win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(Win32State, InputRecordingIndex);
    
    if(ReplayBuffer->MemoryBlock)
    {
        Win32State->InputRecordingIndex = InputRecordingIndex;

        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(Win32State, true, InputRecordingIndex, sizeof(FileName), FileName);
        Win32State->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

#if 0 
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = Win32State->TotalSize;
        SetFilePointerEx(Win32State->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif
        CopyMemory(ReplayBuffer->MemoryBlock, Win32State->GameMemoryBlock, Win32State->TotalSize);
    }
}

internal void
Win32EndRecordingInput(win32_state* Win32State)
{
   CloseHandle(Win32State->RecordingHandle);
   Win32State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayBack(win32_state* Win32State, int InputPlayingIndex)
{
    win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(Win32State, InputPlayingIndex);
    
    if(ReplayBuffer->MemoryBlock){
        Win32State->InputPlayingIndex = InputPlayingIndex;

        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(Win32State, true, InputPlayingIndex, sizeof(FileName), FileName);
        Win32State->PlaybackHandle = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = Win32State->TotalSize;
        SetFilePointerEx(Win32State->PlaybackHandle, FilePosition, 0, FILE_BEGIN);
#endif
        CopyMemory(Win32State->GameMemoryBlock, ReplayBuffer->MemoryBlock, Win32State->TotalSize);
    }
}

internal void
Win32EndInputPlayBack(win32_state* Win32State)
{
   CloseHandle(Win32State->PlaybackHandle);
   Win32State->InputPlayingIndex = 0;
}


internal void
Win32RecordInput(win32_state* Win32State, game_input* NewInput)
{
    DWORD BytesWritten;
    WriteFile(Win32State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void
Win32PlayBackInput(win32_state* Win32State, game_input* NewInput)
{
    DWORD BytesRead = 0;
    if(ReadFile(Win32State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
    {
        if(BytesRead == 0)
        {
            int PlayingIndex = Win32State->InputPlayingIndex;
            Win32EndRecordingInput(Win32State);
            Win32BeginInputPlayBack(Win32State, PlayingIndex);
            ReadFile(Win32State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
        }
    }
}

internal void
ToggleFullScreen(HWND Window)
{
    /*
     * https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
     * */
    DWORD dwStyle = GetWindowLong(Window, GWL_STYLE);
    if (dwStyle & WS_OVERLAPPEDWINDOW) 
    {
        MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
            GetMonitorInfo(MonitorFromWindow(Window,
                           MONITOR_DEFAULTTOPRIMARY), &MonitorInfo)) {
          SetWindowLong(Window, GWL_STYLE,
                        dwStyle & ~WS_OVERLAPPEDWINDOW);
          SetWindowPos(Window, HWND_TOP,
                       MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                       MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                       MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                       SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } 
    else 
    {
        SetWindowLong(Window, GWL_STYLE,
                      dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPosition);
        SetWindowPos(Window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

internal void Win32ProcessPendingMessage(win32_state* Win32State, game_controller_input* KeyboardController) 
{
	MSG Message;

	while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) 
	{
		switch (Message.message) 
		{
			case WM_QUIT: 
			{
				GlobalRunning = false;
			} break;

			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP: 
			{
				uint32 VKCode = (uint32)Message.wParam;
				bool32 WasDown = ((Message.lParam & (1ul << 30)) != 0);
				bool32 IsDown = ((Message.lParam & (1ul << 31)) == 0);

				if (IsDown != WasDown) {
					if (VKCode == 'W') {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
					}
					else if (VKCode == 'A') {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
					}
					else if (VKCode == 'S') {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
					}
					else if (VKCode == 'D') {
						Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
					}
					else if (VKCode == 'Q') {
						Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
					}
					else if (VKCode == 'E') {
						Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
					}
					else if (VKCode == VK_UP) {
						Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
					}
					else if (VKCode == VK_LEFT) {
						Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
					}
					else if (VKCode == VK_DOWN) {
						Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
					}
					else if (VKCode == VK_RIGHT) {
						Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
					}
					else if (VKCode == VK_SPACE) {
						Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
					}
					else if (VKCode == VK_ESCAPE) {
						Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
					}
#if HANDMADE_INTERNAL
                    else if (VKCode == 'P') {
						if (IsDown) {
							GlobalPause = !GlobalPause;
						}
					}
                    else if (VKCode == 'L'){
                        if(IsDown){
                            if(Win32State->InputPlayingIndex == 0){
                                if(Win32State->InputRecordingIndex == 0)
                                {
                                    Win32BeginRecordingInput(Win32State, 1);
                                }
                                else
                                {
                                    Win32EndRecordingInput(Win32State);
                                    Win32BeginInputPlayBack(Win32State, 1);
                                }
                            }
                            else
                            {
                                Win32EndInputPlayBack(Win32State);
                            }
                        }
                    }
#endif
                    if(IsDown){
                        bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                        if ((VKCode == VK_F4) && AltKeyWasDown) {
                            GlobalRunning = false;
                        }
                        if((VKCode == VK_RETURN) && AltKeyWasDown)
                        {
                            if(Message.hwnd)
                            {
                                ToggleFullScreen(Message.hwnd);
                            }
                        }
                    }
				}	
			}break;
			default: {
				TranslateMessage(&Message);
				DispatchMessageA(&Message);
			}break;

		}
	}
}


LRESULT CALLBACK 
Win32WinprocCallback(HWND Window, UINT Message, WPARAM wParam, LPARAM LParam) 
{
	LRESULT Result = 0;
	
	switch (Message) {
		case WM_CLOSE:
		{
			//TODO: Handle this with a message to user?
			GlobalRunning = false;
		} break;
        case WM_SETCURSOR:
        {
           if(DEBUGGlobalShowCursor)
           {
               Result = DefWindowProcA(Window, Message, wParam, LParam);
           }
           else
           {
               SetCursor(0);
           }
        } break;
		case WM_DESTROY:
		{
			//TODO: Destroy windows as error occures and recreate it?
			GlobalRunning = false;
		} break;
		case WM_ACTIVATEAPP:
		{
#if 0
			if(wParam == TRUE)
            {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
            }
            else
            {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 64, LWA_ALPHA);
            }
#endif
		} break;
		
		case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"Keyboard input came in through a non-dispatch message!");
        } break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;

			HDC DeviceContext = BeginPaint(Window, &Paint);

			win32_window_dimensions dimensions = Win32GetWindowDimensions(Window);
			Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, dimensions.Width, dimensions.Height);
			EndPaint(Window, &Paint);
		}break;

		default:
		{
			Result = DefWindowProc(Window, Message, wParam, LParam);
		} break;
	}

	return Result;
}



inline LARGE_INTEGER 
Win32GetWallClock() {
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return Result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
	real32 Result = ((real32)(End.QuadPart - Start.QuadPart) / 
					 (real32)(GlobalPerfCountFrequency));
	return Result;
}

#if 0
internal void Win32DebugDrawVertival(win32_offscreen_buf* BackBuffer, int X, int Top, int Bottom, uint32 Color)
{
	if (Top <= 0) Top = 0;
	if (Bottom > BackBuffer->Height) Bottom = BackBuffer->Height;

	if ((X >= 0) && (X < BackBuffer->Width)) 
	{
		uint8* Pixel = ((uint8*)BackBuffer->Memory +
			(X * BackBuffer->BytesPerPixel) +
			(Top * BackBuffer->Pitch));

		for (int Y = Top; Y < Bottom; ++Y) {
			*(uint32*)Pixel = Color;
			Pixel += BackBuffer->Pitch;
		}
	}
	
}

internal void Win32DrawSoundBufferMarker(win32_offscreen_buf* BackBuffer, win32_sound_output* SoundOutput,real32 C, int PadX, int Top, int Bottom, DWORD PlayCursorValue, uint32 Color)
{
	real32 XReal32 = (C * (real32)PlayCursorValue);
	int X = PadX + (int)(XReal32);
	Win32DebugDrawVertival(BackBuffer, X, Top, Bottom, Color);
}

internal void Win32DebugSyncDisplay(win32_offscreen_buf* BackBuffer, win32_sound_output* SoundOutput, int CurrentMarkerIndex, int MarkerCount, win32_debug_time_marker* Markers, real32 TargetSecondsPerFrame) 
{
	int PadX = 16;
	int PadY = 16;

	int LineHeight = 64;
	

	real32 C = (real32)(BackBuffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
	for (int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
	{
		win32_debug_time_marker* ThisMarker = &Markers[MarkerIndex];
		Assert(ThisMarker->OutputPlayCursor < (DWORD)SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputWriteCursor < (DWORD)SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputLocation < (DWORD)SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputByteCount < (DWORD)SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipPlayCursor < (DWORD)SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipWriteCursor < (DWORD)SoundOutput->SecondaryBufferSize);
		int Top = PadY;
		int Bottom = PadY + LineHeight;

		DWORD PlayCursorColor = 0xFFFFFFFF;
        DWORD WriteCursorColor = 0xFFFF0000;
        DWORD ExpectedFlipColor = 0xFFFFFF00;
        DWORD PlayWindowColor = 0xFFFF00FF;
		
		if (MarkerIndex == CurrentMarkerIndex)
		{
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;

            int FirstTop = Top;
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayCursorColor);
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteCursorColor);

			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayCursorColor);
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteCursorColor);

			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;

			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
		}

		
		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayCursorColor);
		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480 * SoundOutput->BytesPerSample, PlayWindowColor);
		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteCursorColor);
	}
}

#endif

internal void
Win32GetEXEFileName(win32_state* State)
{
    DWORD SizeOfFilename = GetModuleFileNameA(0, State->EXEFileName, sizeof(State->EXEFileName));
    char* OnePastLastSlash = State->EXEFileName;
    for(char* Scan = State->EXEFileName; *Scan; ++Scan)
    {
        if(*Scan == '\\')
        {
            State->OnePastLastEXEFileNameSlash = Scan + 1;
        }
    }
}

int CALLBACK WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR CommandLine,
	int ShowCode) 
{
    win32_state Win32State = {};			
    Win32GetEXEFileName(&Win32State);

    char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, "handmade.dll", sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);
        
    char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, "handmade_temp.dll", sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);
    
    char GameCodeLockFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, "lock.tmp", sizeof(GameCodeLockFullPath), TempGameCodeDLLFullPath);

	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	UINT DesiredSchedulerMS = 1;
	bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

	Win32LoadXInput();
	
	WNDCLASS WindowClass = {};

#if HANDMADE_INTERNAL
    DEBUGGlobalShowCursor = true;
#endif

    // 1000p Display mode is 1920x1000
	Win32ResizeDIBSection(&GlobalBackBuffer, 960, 540);
	
	WindowClass.style			= CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc		= Win32WinprocCallback;
	WindowClass.hInstance		= Instance;
    WindowClass.hCursor         = LoadCursorA(0, IDC_ARROW);
	// WindowClass.hIcon;
	WindowClass.lpszClassName	= "HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass)) 
    {
		HWND WindowHandle = CreateWindowEx(
            0,//WS_EX_TOPMOST|WS_EX_LAYERED,
            WindowClass.lpszClassName,
			"HandmadeHero",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			0, 0, Instance, 0);

		if (WindowHandle) 
        {
            GlobalRunning = true;
            HDC RefreshDC = GetDC(WindowHandle);
            int RefreshMonitorHz = 60;
            int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            if(Win32RefreshRate > 1)
            {
                RefreshMonitorHz = Win32RefreshRate;
            }
            real32 GameUpdateHz = (RefreshMonitorHz / 2.0f);
	        real32 TargetSecondsPerFrame = (1.0f / (real32)GameUpdateHz);

			win32_sound_output SoundOutput	= {};
			SoundOutput.SamplesPerSeconds	= 48000; //48kHz
			SoundOutput.BytesPerSample		= sizeof(int16) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSeconds * SoundOutput.BytesPerSample;
			SoundOutput.SafetyBytes = (int)(((real32)SoundOutput.SamplesPerSeconds * (real32)SoundOutput.BytesPerSample / GameUpdateHz) / 3.0f);
			
			Win32InitDSound(WindowHandle, SoundOutput.SamplesPerSeconds, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondarySBuffer->Play(0, 0, DSBPLAY_LOOPING);

#if 0
			while (GlobalRunning) {
				DWORD PlayCursor;
				DWORD WriteCursor;
				GlobalSecondarySBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
				char SoundOutBuffer[256];
				_snprintf_s(SoundOutBuffer, sizeof(SoundOutBuffer), 
							"PC:%u, WC:%u\n", 
							PlayCursor, WriteCursor);

				OutputDebugStringA(SoundOutBuffer);
			}
#endif
			int16* Samples = (int16*)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
													MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			//TODO: Check for value to be defined on 
#if HANDMADE_INTERNAL
			LPVOID BaseAddress = (LPVOID)TeraBytes_(2);
#else
			LPVOID BaseAddress = 0;
#endif

			game_memory GameMemory          = {};
			GameMemory.PermanentStorageSize = MegaBytes_(64);
			GameMemory.TransientStorageSize = GigaBytes_(1);
			
			Win32State.TotalSize        = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
			Win32State.GameMemoryBlock  = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize, 
														MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			
		    GameMemory.PermanentStorage             = Win32State.GameMemoryBlock;	
			GameMemory.TransientStorage             = ((uint8*)GameMemory.PermanentStorage + 
											            GameMemory.PermanentStorageSize);
            GameMemory.DEBUGPlatformFreeFileMemory  = DEBUGPlatformFreeFileMemory;
            GameMemory.DEBUGPlatformReadEntireFile  = DEBUGPlatformReadEntireFile;
            GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

            for(int ReplayIndex = 0; ReplayIndex < ArrayCount(Win32State.ReplayBuffers); ++ReplayIndex)
            {
                win32_replay_buffer* ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];
                
                Win32GetInputFileLocation(&Win32State, false, ReplayIndex, sizeof(ReplayBuffer->ReplayFileName), ReplayBuffer->ReplayFileName);

                ReplayBuffer->Handle = CreateFileA(ReplayBuffer->ReplayFileName, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

                LARGE_INTEGER MaxSize;
                MaxSize.QuadPart = Win32State.TotalSize;
                ReplayBuffer->MemoryMap = CreateFileMapping(ReplayBuffer->Handle, 0, PAGE_READWRITE, MaxSize.HighPart, MaxSize.LowPart, 0);
				DWORD Error = GetLastError();
				ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, Win32State.TotalSize);
			    Error = GetLastError();
                if(ReplayBuffer->MemoryBlock)
                {

                }
                else
                {
                    // Diagnostic
                }
            }

			if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage) 
            {
				game_input Input[2]			= {};
				game_input* NewInput		= &Input[0];
				game_input* OldInput		= &Input[1];

				LARGE_INTEGER LastCounter	= Win32GetWallClock();
			    LARGE_INTEGER FlipWallClock = Win32GetWallClock();	


				int DebugTimeMarkerIndex	= 0;
				win32_debug_time_marker DebugTimeMarkers[30] = {0};

				DWORD AudioLatencyBytes		= 0;
				real32 AudioLatencySeconds	= 0;
				bool32 SoundIsValid         = false;
				uint64 LastCycleCount	    = __rdtsc();
                
                Win32State.InputRecordingIndex     = 0;
                Win32State.InputPlayingIndex       = 0;
                uint32 LoadCounter                 = 0;

                win32_game_code GameCode = Win32LoadGameCode(SourceGameCodeDLLFullPath, 
                                                             TempGameCodeDLLFullPath, 
                                                             GameCodeLockFullPath);
				while (GlobalRunning) 
				{
                    NewInput->dtForFrame        = TargetSecondsPerFrame;
                    FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
                    if(CompareFileTime(&NewDLLWriteTime, &GameCode.LastWriteTime) != 0)
                    {
                        Win32UnloadGameCode(&GameCode);
                        GameCode = Win32LoadGameCode(SourceGameCodeDLLFullPath, 
                                                     TempGameCodeDLLFullPath, 
                                                     GameCodeLockFullPath);
                        LoadCounter = 0;
                    }
                    					
                    game_controller_input* OldKeyboardController = GetController(OldInput, 0);
					game_controller_input* NewKeyboardController = GetController(NewInput, 0);
					*NewKeyboardController = {};
					NewKeyboardController->IsConnected = true;

					for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex) 
					{
						NewKeyboardController->Buttons[ButtonIndex].EndedDown =
								OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}

					Win32ProcessPendingMessage(&Win32State, NewKeyboardController);
					
					if(!GlobalPause)
					{
                        POINT MouseP;
                        GetCursorPos(&MouseP);
                        ScreenToClient(WindowHandle, &MouseP);
                        NewInput->MouseX = MouseP.x;
                        NewInput->MouseY = MouseP.y;
                        NewInput->MouseZ = 0;

                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1], GetKeyState(VK_RBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2], GetKeyState(VK_MBUTTON) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
                        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));

						DWORD MaxControllerCount = XUSER_MAX_COUNT;
						if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
						{
							MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
						}

						for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex) 
                        {
							DWORD OurControllerIndex = ControllerIndex + 1;
							game_controller_input* OldController = GetController(OldInput, OurControllerIndex);
							game_controller_input* NewController = GetController(NewInput, OurControllerIndex);

							XINPUT_STATE ControllerState;
							if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) 
							{
								NewController->IsConnected = true;
                                NewController->IsAnalog = OldController->IsAnalog;

								XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

								NewController->StickAvarageX = Win32ProcessXInputStickValue(Pad->sThumbLX,
																		XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								NewController->StickAvarageY = Win32ProcessXInputStickValue(Pad->sThumbLY,
																		XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

								if ((NewController->StickAvarageX != 0.0f) ||
									(NewController->StickAvarageY != 0.0f)) 
									{
									NewController->IsAnalog = true;
								}

								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) 
								{
									NewController->StickAvarageY = 1.0f;
									NewController->IsAnalog = false;
								}
								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) 
								{
									NewController->StickAvarageY = -1.0f;
									NewController->IsAnalog = false;
								}
								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) 
								{
									NewController->StickAvarageX = -1.0f;
									NewController->IsAnalog = false;
								}
								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) 
								{
									NewController->StickAvarageX = 1.0f;
									NewController->IsAnalog = false;
								}
								
								real32 Threshold = 0.5;
								Win32ProccesXInputButton(&NewController->MoveLeft, &OldController->MoveLeft, 
														1, (NewController->StickAvarageX < -Threshold ? 1 : 0));
								Win32ProccesXInputButton(&NewController->MoveRight, &OldController->MoveRight, 
														1, (NewController->StickAvarageX > Threshold ? 1 : 0));
								Win32ProccesXInputButton(&NewController->MoveDown, &OldController->MoveDown, 
														1, (NewController->StickAvarageY < -Threshold ? 1 : 0));
								Win32ProccesXInputButton(&NewController->MoveUp, &OldController->MoveUp, 
														1, (NewController->StickAvarageY > Threshold ? 1 : 0));

								Win32ProccesXInputButton(&NewController->ActionDown, &OldController->ActionDown, 
															XINPUT_GAMEPAD_A, Pad->wButtons);
								Win32ProccesXInputButton(&NewController->ActionRight, &OldController->ActionRight, 
															XINPUT_GAMEPAD_B, Pad->wButtons);
								Win32ProccesXInputButton(&NewController->ActionLeft, &OldController->ActionLeft, 
															XINPUT_GAMEPAD_X, Pad->wButtons);
								Win32ProccesXInputButton(&NewController->ActionUp, &OldController->ActionUp, 
															XINPUT_GAMEPAD_Y, Pad->wButtons);
								Win32ProccesXInputButton(&NewController->LeftShoulder, &OldController->LeftShoulder, 
															XINPUT_GAMEPAD_LEFT_SHOULDER, Pad->wButtons);
								Win32ProccesXInputButton(&NewController->RightShoulder, &OldController->RightShoulder, 
															XINPUT_GAMEPAD_RIGHT_SHOULDER, Pad->wButtons);

								Win32ProccesXInputButton(&NewController->Start, &OldController->Start, XINPUT_GAMEPAD_START, Pad->wButtons);
								Win32ProccesXInputButton(&NewController->Back, &OldController->Back, XINPUT_GAMEPAD_BACK, Pad->wButtons);
							}
							else 
                            {
								NewController->IsConnected = false;
							}
						}
						thread_context Thread = {};

						game_offscreen_buf BackBuffer = {};
						BackBuffer.Memory 	          = GlobalBackBuffer.Memory;
						BackBuffer.Width 	          = GlobalBackBuffer.Width;
						BackBuffer.Height 	          = GlobalBackBuffer.Height;
						BackBuffer.Pitch 	          = GlobalBackBuffer.Pitch;
                        BackBuffer.BytesPerPixel      = GlobalBackBuffer.BytesPerPixel;
                        if(Win32State.InputRecordingIndex)
                        {
                            Win32RecordInput(&Win32State, NewInput);
                        }
                        if(Win32State.InputPlayingIndex)
                        {
                            Win32PlayBackInput(&Win32State, NewInput);
                        }
                        if(GameCode.UpdateAndRender)
                        {
						    GameCode.UpdateAndRender(&Thread, &GameMemory, NewInput, &BackBuffer);
                        }

                        LARGE_INTEGER AudioCounter     = Win32GetWallClock();
						real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioCounter);


						DWORD PlayCursor;
						DWORD WriteCursor;
						if (GlobalSecondarySBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
						{
							
							if (!SoundIsValid) 
                            {
								SoundOutput.RunningSampleIndex = (WriteCursor / SoundOutput.BytesPerSample);
								SoundIsValid = true;
							}

							DWORD ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
								SoundOutput.SecondaryBufferSize); //Sound sample indexing

							DWORD ExpectedSoundBytesPerFrame = (int)(((real32)SoundOutput.SamplesPerSeconds * (real32)SoundOutput.BytesPerSample) / GameUpdateHz);
                            real32 SecondsLeftUntilWeFlip    = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
                            DWORD ExpectedBytesUntilFlip     = (DWORD)((SecondsLeftUntilWeFlip / TargetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame);
							DWORD ExpectedFrameBoundaryByte  = (PlayCursor + ExpectedSoundBytesPerFrame);

							DWORD SafeWriteCursor = WriteCursor;
							if (SafeWriteCursor < PlayCursor) 
                            {
								SafeWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							Assert(SafeWriteCursor >= PlayCursor);
							SafeWriteCursor += SoundOutput.SafetyBytes;

							bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);
							
							DWORD TargetCursor = 0;
							if (AudioCardIsLowLatency) 
                            {
								TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
							}
							else 
                            {
								TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
							}
							TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);

							DWORD BytesToWrite = 0;
							if (ByteToLock > TargetCursor) 
							{
								BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
								BytesToWrite += TargetCursor;
							}
							else 
							{
								BytesToWrite = TargetCursor - ByteToLock;
							}

							game_sound_buffer SoundBuffer = {};
							SoundBuffer.SamplesPerSeconds = SoundOutput.SamplesPerSeconds;
							SoundBuffer.SamplesCount      = BytesToWrite / SoundOutput.BytesPerSample;
							SoundBuffer.Samples           = Samples;
                            if(GameCode.GetSoundSamples)
                            {
							    GameCode.GetSoundSamples(&Thread, &GameMemory, &SoundBuffer);
                            }
#if HANDMADE_INTERNAL

							win32_debug_time_marker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
							Marker->OutputPlayCursor        = PlayCursor;
							Marker->OutputWriteCursor       = WriteCursor;
							Marker->OutputLocation          = ByteToLock;
							Marker->OutputByteCount         = BytesToWrite;
                            Marker->ExpectedFlipPlayCursor  = ExpectedFrameBoundaryByte;

							DWORD UnwrappedWriteCursor = WriteCursor;
							if (UnwrappedWriteCursor < PlayCursor) 
                            {
								UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							
							AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
							AudioLatencySeconds = 
								(((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) /
								(real32)SoundOutput.SamplesPerSeconds);
#if 0
							char SoundOutBuffer[256];
							_snprintf_s(SoundOutBuffer, sizeof(SoundOutBuffer), 
								"PC:%u, BTL:%u, TC:%u, BTW:%u, DELTA:%u (%fs)\n",
								PlayCursor, ByteToLock, TargetCursor, BytesToWrite, 
								AudioLatencyBytes, AudioLatencySeconds);

							OutputDebugStringA(SoundOutBuffer);
#endif
#endif
							Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);							
						}
						else
                        {
							SoundIsValid = false;
						}
						

						LARGE_INTEGER WorkCounter = Win32GetWallClock();
						real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

						real32 SecondsElapsedForFrame = WorkSecondsElapsed;
						if (SecondsElapsedForFrame < TargetSecondsPerFrame) 
						{
							if (SleepIsGranular) 
							{
								DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - 
																   SecondsElapsedForFrame));

								if (SleepMS > 0) 
								{
									Sleep(SleepMS);
								}
							}
							real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, 
																						Win32GetWallClock());
								
							if (TestSecondsElapsedForFrame < TargetSecondsPerFrame) 
                            {
								// Missed Sleep Logging
							}

							while (SecondsElapsedForFrame < TargetSecondsPerFrame) 
                            {
								SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
							}
						}
						else 
						{
							//Missed Frame rate
							//Logging
						}

						LARGE_INTEGER EndCounter = Win32GetWallClock();
						real32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
						LastCounter = EndCounter;

						win32_window_dimensions dimensions = Win32GetWindowDimensions(WindowHandle);
                        
                        HDC MainWindow = GetDC(WindowHandle);
						Win32DisplayBufferInWindow(&GlobalBackBuffer, MainWindow, 
													dimensions.Width, dimensions.Height);

					    ReleaseDC(WindowHandle, MainWindow);	 
						FlipWallClock = Win32GetWallClock();


#if HANDMADE_INTERNAL
						{
							DWORD PlayCursor;
							DWORD WriteCursor;
							if (GlobalSecondarySBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK) 
							{
								Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
								win32_debug_time_marker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
								Marker->FlipPlayCursor = PlayCursor;
								Marker->FlipWriteCursor = WriteCursor;
							}
						}
#endif


						game_input* Temp = NewInput;
						NewInput         = OldInput;
						OldInput         = Temp;
#if 0
						uint64 EndCycleCount = __rdtsc();
						uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
						LastCycleCount       = EndCycleCount;

						//real32 MSPerFrame = (int32)((1000.0f * (real32)CounterElapsed) / GlobalPerfCountFrequency); //(x counts / 1(frame))/(counts per seconds)
						real64 FPS  = 0.0f;
						real64 MCPF = ((real64)CyclesElapsed / (1000.0f * 1000.0f)); //MilliCycles per Frame

						char TimeOutBuffer[256];
						_snprintf_s(TimeOutBuffer, sizeof(TimeOutBuffer), "%.05fms/f, %.02ff/s %.02fmc/f\n", MSPerFrame, FPS, MCPF);

						OutputDebugStringA(TimeOutBuffer);
#endif
#if HANDMADE_INTERNAL
						++DebugTimeMarkerIndex;
						if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers)) 
						{
							DebugTimeMarkerIndex = 0;
						}
					}
#endif
				}
			}
			
		}
		else {
			//TODO: Logging
		}
	}
	else {
		//TODO: Logging
	}

	
	return 0;
}
