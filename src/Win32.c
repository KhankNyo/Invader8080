
#include <windows.h>
#include <dsound.h>
#include "Invaders.c"
#include <math.h>


#define PI 3.14159265f
#define METHOD_CALL(pObj, Meth) (pObj)->lpVtbl->Meth
#define DESTRUCT(pObj) if (NULL != (pObj)) METHOD_CALL(pObj, Release)(pObj)
#define WIN32_FN_DIRECT_SOUND_CREATE(name)\
    HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter)


static uint8_t sBackBuffer[256*224 * 4];

static HWND sMainWindow;
static LPDIRECTSOUNDBUFFER sSecondaryBuffer;
static WAVEFORMATEX *sSoundFormat;
static int16_t *sSoundBufferPtr;
static uint32_t sSoundBufferSize;



static void Win32_Fatal(const wchar_t *ErrorMessage)
{
    MessageBoxW(NULL, ErrorMessage, L"Fatal Error", MB_ICONERROR);
    ExitProcess(1);
}


LRESULT CALLBACK Win32_WndProc(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    switch (Msg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    } break;
    case WM_KEYLAST:
    case WM_KEYUP:
    {
        Invader_OnKeyUp(WParam);
    } break;
    case WM_KEYDOWN:
    {
        Invader_OnKeyDown(WParam);
    } break;
    case WM_PAINT:
    {
        Platform_SwapBuffer();
    } break;
    default: 
    {
        return DefWindowProcW(Window, Msg, WParam, LParam);
    } break;
    }
    return 0;
}


static BOOL Win32_PollInputs(void)
{
    MSG Msg;
    while (PeekMessageW(&Msg, 0, 0, 0, PM_REMOVE))
    {
        if (Msg.message == WM_QUIT)
            return 0;

        TranslateMessage(&Msg);
        DispatchMessageW(&Msg);
    }
    return 1;
}

static Bool8 Win32_InitDirectSound(HWND Window)
{
    HMODULE DirectSoundLibrary = LoadLibraryA("dsound.dll");
    if (NULL == DirectSoundLibrary)
    {
        return false;
    }
    typedef WIN32_FN_DIRECT_SOUND_CREATE(Win32FnDirectSoundCreate);
    Win32FnDirectSoundCreate *DirectSoundCreate = (Win32FnDirectSoundCreate*)
        GetProcAddress(DirectSoundLibrary, "DirectSoundCreate");

    /* calling ctor */
    LPDIRECTSOUND DirectSound;
    if (NULL == DirectSoundCreate 
    || DirectSoundCreate(NULL, &DirectSound, NULL))
    {
        return false;
    }

    /* set co-op level */
    if (FAILED(METHOD_CALL(DirectSound, 
        SetCooperativeLevel(DirectSound, Window, DSSCL_PRIORITY)
    )))
    {
        return false;
    }

    /* create primary buffer */
    DSBUFFERDESC PrimaryBufferDescription = {
        .dwSize = sizeof PrimaryBufferDescription,
        .dwFlags = DSBCAPS_PRIMARYBUFFER,
    };
    LPDIRECTSOUNDBUFFER PrimaryBuffer;
    if (FAILED(METHOD_CALL(DirectSound, 
        CreateSoundBuffer(DirectSound, &PrimaryBufferDescription, &PrimaryBuffer, NULL)
    )))
    {
        return false;
    }

    /* create secondary buffer */
    /* specify the sound format: PCM 16 bit */
#define SAMPLES_PER_SEC 44100
    static int16_t SoundBuffer[SAMPLES_PER_SEC * 2];
    static WAVEFORMATEX SoundFormatConfig = {
        .wBitsPerSample     = 16,
        .nChannels          = 2, /* stereo */
        .wFormatTag         = WAVE_FORMAT_PCM,
        .nSamplesPerSec     = SAMPLES_PER_SEC,
        .nAvgBytesPerSec    = sizeof SoundBuffer,
        .nBlockAlign        = 2 * sizeof(int16_t),
    };
#undef SAMPLES_PER_SEC
    if (FAILED(METHOD_CALL(PrimaryBuffer, 
        SetFormat(PrimaryBuffer, &SoundFormatConfig)
    )))
    {
        return false;
    }

    DSBUFFERDESC SecondaryBufferDescription = {
        .dwSize = sizeof SecondaryBufferDescription,
        /* don't set flags here, we don't need any */
        .lpwfxFormat = &SoundFormatConfig,
        .dwBufferBytes = sizeof SoundBuffer,
    };
    if (FAILED(METHOD_CALL(DirectSound, 
        CreateSoundBuffer(DirectSound, &SecondaryBufferDescription, &sSecondaryBuffer, NULL)
    )))
    {
        return false;
    }

    sSoundBufferPtr = SoundBuffer;
    sSoundBufferSize = sizeof SoundBuffer;
    sSoundFormat = &SoundFormatConfig;
    return true;
}

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PWCHAR CmdLine, int CmdShow)
{
    (void)PrevInstance, (void)CmdLine, (void)CmdShow;

    WNDCLASSEXW WindowClass = {
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .cbSize = sizeof WindowClass,
        .hInstance = Instance,
        .lpfnWndProc = Win32_WndProc,
        .lpszClassName = L"Invader", 
        .hCursor = LoadCursorW(Instance, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1),
    };
    RegisterClassExW(&WindowClass);

    sMainWindow = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WindowClass.lpszClassName,
        L"Space Invader",
        WS_OVERLAPPEDWINDOW | WS_BORDER | WS_CAPTION,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1080, 720,
        NULL, NULL, NULL, NULL
    );
    if (NULL == sMainWindow)
    {
        Win32_Fatal(L"Cannot create window");
    }
    ShowWindow(sMainWindow, SW_SHOW);
    /*
     * gcc didn't link correctly or something??? 
     * uncomment resulting in a bunch of NULLs on the stack 
     * so I couldn't view the backtrace either.
     * Works fine with tcc????
     *
     * UpdateWindow(sMainWindow);
     * */

    if (SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED)) 
    && Win32_InitDirectSound(sMainWindow)) 
    {
        METHOD_CALL(sSecondaryBuffer, Play(sSecondaryBuffer, 0, 0, DSBPLAY_LOOPING));
    }
    else
    {
        Platform_PrintError("Unable to initalize DirectSound.");
    }
    

    Invader_Setup();
    double Last = Platform_GetTimeMillisec();
    DWORD LastPlayCursor = -1;
    while (Win32_PollInputs())
    {
        Invader_Loop();

        double Current = Platform_GetTimeMillisec();
        if (Current - Last >= 1000.0 / 60 
        && NULL != sSoundBufferPtr)
        {
            Last = Current;

            DWORD PlayCursor, WriteCursor;
            if (SUCCEEDED(METHOD_CALL(sSecondaryBuffer, 
                GetCurrentPosition(sSecondaryBuffer, &PlayCursor, &WriteCursor)
            )) && PlayCursor != LastPlayCursor)
            {
                LastPlayCursor = PlayCursor;
                DWORD BytesToLock = 0;
                if (PlayCursor < WriteCursor)
                    BytesToLock = PlayCursor + sSoundBufferSize - WriteCursor;
                else if (WriteCursor < PlayCursor)
                    BytesToLock = PlayCursor - WriteCursor;

                DWORD FirstRegionSize, SecondRegionSize;
                LPVOID FirstRegion, SecondRegion;
                if (SUCCEEDED(METHOD_CALL(sSecondaryBuffer, 
                    Lock(sSecondaryBuffer, 
                        0, 0, 
                        &FirstRegion, &FirstRegionSize,
                        &SecondRegion, &SecondRegionSize, 
                        DSBLOCK_ENTIREBUFFER
                    )
                )))
                {
                    /* sine wave */
                    static uint32_t SoundBufferIndex = 0;
                    SoundBufferIndex %= sSoundBufferSize;
                    uint32_t Hz = 440;
                    float SampleDuration = (float)sSoundFormat->nSamplesPerSec / (float)Hz;
                    float SoundVolume = 3000;
                    int SampleSize = sSoundFormat->nBlockAlign;
                    int16_t *SoundRegion = FirstRegion;
                    for (DWORD i = 0; i < FirstRegionSize / SampleSize; i++)
                    {
                        float x = 2.0 * PI * ((float)SoundBufferIndex / SampleDuration);
                        int16_t SoundData = SoundVolume * sinf(x);
                        /* left, right */
                        *SoundRegion++ = SoundData;
                        *SoundRegion++ = SoundData;

                        SoundBufferIndex++;
                    }

                    SoundRegion = SecondRegion;
                    for (DWORD i = 0; i < SecondRegionSize / SampleSize; i++)
                    {
                        float x = 2.0 * PI * ((float)SoundBufferIndex / SampleDuration);
                        int16_t SoundData = SoundVolume * sinf(x);
                        /* left, right */
                        *SoundRegion++ = SoundData;
                        *SoundRegion++ = SoundData;

                        SoundBufferIndex++;
                    }
                    METHOD_CALL(sSecondaryBuffer, 
                        Unlock(sSecondaryBuffer, 
                            FirstRegion, FirstRegionSize, 
                            SecondRegion, SecondRegionSize
                        )
                    );
                }
            }
        }
    }
    /* dont't need to cleanup the windows, Windows does it for us */
    ExitProcess(0);
}




double Platform_GetTimeMillisec(void)
{
    return GetTickCount64();
}

void *Platform_GetBackBuffer(void)
{
    return sBackBuffer;
}

void Platform_SwapBuffer(void)
{
    /* force Windows to redraw the window */
    InvalidateRect(sMainWindow, NULL, FALSE);

    PAINTSTRUCT PaintStruct;
    HDC DeviceContext = BeginPaint(sMainWindow, &PaintStruct);
    RECT Rect = PaintStruct.rcPaint;

    int DstX = Rect.left, 
        DstY = Rect.top,
        DstW = Rect.right - Rect.left,
        DstH = Rect.bottom - Rect.top;
    double AspectRatio = (double)DstW / DstH;
    /* window is wider than game */
    if (AspectRatio > 224.f / 256.f)
    {
        /* scale width to fit */
        DstW = DstH * 224.f / 256.f;
        int Middle = (Rect.right - Rect.left) / 2;
        DstX = DstX + Middle - DstW/2;
    }
    else /* game is wider */
    {
        /* scale height to fit */
        DstH = DstW * 256.f / 224.f;
        int Middle = (Rect.bottom - Rect.top) / 2;
        DstY = DstY + Middle - DstH/2;
    }

    static const BITMAPINFO BitmapInfo = {
        .bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader),
        .bmiHeader.biWidth = 224,
        .bmiHeader.biHeight = -256, /* reverse, buffer going from top to bottom */
        .bmiHeader.biPlanes = 1,
        .bmiHeader.biBitCount = 32,
        .bmiHeader.biCompression = BI_RGB,
    };

    StretchDIBits(DeviceContext, 
        DstX, DstY, DstW, DstH, 
        0, 0, 224, 256, 
        sBackBuffer, &BitmapInfo, 
        DIB_RGB_COLORS, SRCCOPY
    );

    EndPaint(sMainWindow, &PaintStruct);
}


PlatformSoundBuffer *Platform_RetrieveSoundBuffer(void)
{
    return NULL;
}

void Platform_ClearSoundBuffer(PlatformSoundBuffer *Sound)
{
    if (NULL == Sound->Buffer)
        return;

    for (size_t i = 0; i < Sound->BufferSizeBytes / sizeof Sound->Buffer[0]; i++)
    {
        Sound->Buffer[i] = 0;
    }

}

void Platform_MixSoundBuffer(PlatformSoundBuffer *Sound, const void *Data, size_t DataSize)
{
    size_t MixingSize = DataSize;
    if (Sound->BufferSizeBytes < MixingSize)
        MixingSize = Sound->BufferSizeBytes;

    const int16_t *SampleData = Data;
    MixingSize /= 2;
    for (unsigned i = 0; i < MixingSize; i++)
    {
        int32_t Result = Sound->Buffer[i] + SampleData[i];
        if (Result > INT16_MAX)
            Result = INT16_MAX;
        else if (Result < INT16_MIN)
            Result = INT16_MIN;
        Sound->Buffer[i] = Result;
    }
}

void Platform_CommitSoundBuffer(PlatformSoundBuffer *Sound)
{
}


void Platform_Exit(int ExitCode)
{
    ExitProcess(ExitCode);
}

void Platform_PrintError(const char *ErrorMessage)
{
    MessageBoxA(NULL, ErrorMessage, "Error", MB_ICONERROR);
}

