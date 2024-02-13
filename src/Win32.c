
#include <windows.h>
#include <dsound.h>
#include "Invaders.c"
#include <math.h>


#define PI 3.14159265f
#define METHOD_CALL(pObj, Meth) (pObj)->lpVtbl->Meth
#define DESTRUCT(pObj) if (NULL != (pObj)) METHOD_CALL(pObj, Release)(pObj)
#define WIN32_FN_DIRECT_SOUND_CREATE(name)\
    HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter)

typedef struct Win32_SoundConfig
{
    uint32_t SamplesPerSec;
    uint32_t SampleSize;
    uint32_t SampleDuration;
    uint32_t BufferSize;
} Win32_SoundConfig;

static uint8_t sBackBuffer[256*224 * 4];
static int16_t sSoundBuffer[48000 * 2];
static HWND sMainWindow;
static LPDIRECTSOUNDBUFFER sWin32_SecondarySoundBuffer;
static PlatformSoundBuffer sSound;
static Win32_SoundConfig *sSoundConfig;


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


static int Win32_InitDirectSound(HWND Window, uint32_t SamplePerSec, uint32_t BufferSize)
{
    /* load DirectSound lib */
    HMODULE LibDSound = LoadLibraryA("dsound.dll");
    if (NULL == LibDSound)
    {
        /* TODO: logging */
        return 0;
    }


    /* load DirectSound library */
    typedef WIN32_FN_DIRECT_SOUND_CREATE(Win32FnDirectSoundCreate);
    Win32FnDirectSoundCreate *DSoundCreate = 
        (Win32FnDirectSoundCreate*)GetProcAddress(LibDSound, "DirectSoundCreate");


    /* Get DirectSound Object */
    LPDIRECTSOUND DSound;
    if (NULL == DSoundCreate || FAILED(DSoundCreate(0, &DSound, 0)))
    {
        /* TODO: logging */
        return 0;
    }


    /* set Co-op level */
    if (FAILED(METHOD_CALL(DSound, SetCooperativeLevel(DSound, Window, DSSCL_PRIORITY))))
    {
        /* TODO: logging */
        return 0;
    }


    /* Create a primary buffer */
    DSBUFFERDESC BufferDescription = {
        .dwSize = sizeof BufferDescription,
        .dwFlags = DSBCAPS_PRIMARYBUFFER,
    };
    LPDIRECTSOUNDBUFFER Primary;
    if (FAILED(METHOD_CALL(DSound, CreateSoundBuffer)(DSound, &BufferDescription, &Primary, 0)))
    {
        /* TODO: logging */
        return 0;
    }

    int NumChannels = 2, BitsPerSample = 16;
    /* [LEFT RIGHT]         [LEFT RIGHT] */
    /* [2bytes 2bytes]      [2bytes 2bytes] */
    /* [4 bytes alignment]  [4 bytes alignment]*/
    int BlockAlignment = NumChannels * BitsPerSample / 8;
    WAVEFORMATEX WaveFormat = {
        .nChannels = NumChannels,
        .wFormatTag = WAVE_FORMAT_PCM,
        .nSamplesPerSec = SamplePerSec,
        .wBitsPerSample = BitsPerSample,
        .nBlockAlign = BlockAlignment,
        .nAvgBytesPerSec = SamplePerSec * BlockAlignment,
    };
    /* fuck this API */
    if (FAILED(METHOD_CALL(Primary, SetFormat(Primary, &WaveFormat))))
    {
        /* TODO: logging */
        return 0;
    }


    /* Create a secondary buffer, something we actually write to */
    BufferDescription = (DSBUFFERDESC) {
        .dwSize = sizeof BufferDescription,
        /* fuck me */
        .dwBufferBytes = BufferSize, 
        .lpwfxFormat = &WaveFormat,
    };
    if (FAILED(METHOD_CALL(DSound, CreateSoundBuffer)(DSound, &BufferDescription, &sWin32_SecondarySoundBuffer, 0)))
    {
        /* TODO: logging */
        return 0;
    }

    /* start rockin */
    return 1;
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

    Win32_SoundConfig LocalSoundConfig = {
        .SamplesPerSec = 48000,
        .SampleSize = sizeof(int16_t)*2,
        .BufferSize = 48000 * sizeof(uint16_t)*2,
        .SampleDuration = 1000 / 60, /* 60 fps */
    };
    if (!Win32_InitDirectSound(sMainWindow, 
        LocalSoundConfig.SamplesPerSec, LocalSoundConfig.BufferSize))
    {
        Platform_PrintError("Unable to initalize DirectSound.");
        sSoundConfig = NULL;
    }
    else
    {
        Platform_ClearSoundBuffer(&sSound);
        METHOD_CALL(sWin32_SecondarySoundBuffer, Play)(sWin32_SecondarySoundBuffer, 0, 0, DSBPLAY_LOOPING);
        sSoundConfig = &LocalSoundConfig;
    }

    Invader_Setup();
    while (Win32_PollInputs())
    {
        Invader_Loop();
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


PlatformSoundBuffer *Platform_RetrieveSoundBuffer(unsigned SampleDurationInMillisec)
{
    if (NULL == sSoundConfig)
    {
        return NULL;
    }
    sSoundConfig->SampleDuration = SampleDurationInMillisec;

    DWORD PlayCursor, WriteCursor;
    if (FAILED(METHOD_CALL(sWin32_SecondarySoundBuffer, 
        GetCurrentPosition)(sWin32_SecondarySoundBuffer, &PlayCursor, &WriteCursor
    )))
    {
        return NULL;
    }

    DWORD BytesToLock = (sSound.WrittenSizeBytes * sSoundConfig->SampleSize)
        % sizeof sSoundBuffer;
    DWORD BytesToWrite = 0;
    if (WriteCursor > PlayCursor)
    {
        DWORD Available = sizeof sSoundBuffer;
        BytesToWrite = Available - BytesToLock;
        BytesToWrite += PlayCursor;
    }
    else if (WriteCursor > PlayCursor)
    {
        BytesToWrite = WriteCursor - PlayCursor;
    }
    sSound.WrittenSizeBytes += BytesToWrite;
    sSound.WrittenSizeBytes %= sSoundConfig->BufferSize;
    sSound.Buffer = sSoundBuffer;
    sSound.SamplePerSec = sSoundConfig->SamplesPerSec;
    sSound.SampleSize = sSoundConfig->SampleSize;
    sSound.BufferSizeBytes = BytesToWrite;
    return &sSound;
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
    /* get the regions inside the sound buffer available for writing */
    DWORD FirstRegionSize,
          SecondRegionSize;
    LPVOID FirstRegion,
           SecondRegion;
    if (SUCCEEDED(METHOD_CALL(sWin32_SecondarySoundBuffer, 
    Lock)(sWin32_SecondarySoundBuffer, 
        Sound->WrittenSizeBytes, Sound->BufferSizeBytes, 
        &FirstRegion, &FirstRegionSize, 
        &SecondRegion, &SecondRegionSize, 
        0
    )))
    {
        int16_t *SampleOut = FirstRegion;
        int16_t *Buffer = Sound->Buffer;
        for (DWORD i = 0; i < FirstRegionSize / Sound->SampleSize; i++)
        {
            *SampleOut++ = *Buffer++;
            *SampleOut++ = *Buffer++;
        }
        SampleOut = SecondRegion;
        for (DWORD i = 0; i < SecondRegionSize / Sound->SampleSize; i++)
        {
            *SampleOut++ = *Buffer++;
            *SampleOut++ = *Buffer++;
        }
        METHOD_CALL(sWin32_SecondarySoundBuffer, 
            Unlock)(sWin32_SecondarySoundBuffer, 
                FirstRegion, FirstRegionSize,
                SecondRegion, SecondRegionSize
        );
    }
}


void Platform_Exit(int ExitCode)
{
    ExitProcess(ExitCode);
}

void Platform_PrintError(const char *ErrorMessage)
{
    MessageBoxA(NULL, ErrorMessage, "Error", MB_ICONERROR);
}

