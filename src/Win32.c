#include <windows.h>
#include "Invaders.c"
#include "Platform.h"




#define METHOD_CALL(pObj, Meth) (pObj)->lpVtbl->Meth
#define DESTRUCT(pObj) if (NULL != (pObj)) METHOD_CALL(pObj, Release)(pObj)
#define WIN32_FN_DIRECT_SOUND_CREATE(name)\
    HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter)
#define SOUND_QUEUE_INITVAL 255


typedef struct PlatformCriticalSection 
{
    CRITICAL_SECTION Sect;
} PlatfromCriticalSection;

typedef struct Win32_SoundThreadData 
{
    PlatformAudioFormat AudioFormat;
    uint8_t *Buffers;
    WAVEHDR *Headers;
} Win32_SoundThreadData;



static HWND sWin32_MainWindow;
static PlatformBackBuffer sWin32_BackBuffer = { 0 };
static BITMAPINFO sWin32_BackBuffer_Bitmap = {
    .bmiHeader.biSize = sizeof(sWin32_BackBuffer_Bitmap.bmiHeader),
    .bmiHeader.biWidth = 0,
    .bmiHeader.biHeight = 0, 
    .bmiHeader.biPlanes = 1,
    .bmiHeader.biBitCount = 32,
    .bmiHeader.biCompression = BI_RGB,
};

static WAVEFORMATEX sWin32_AudioFormat;
static HWAVEOUT sWin32_SoundDevice;
static HANDLE sWin32_SoundThread_Handle = INVALID_HANDLE_VALUE;
static PLATFORM_ATOMIC Bool8 saWin32_SoundThread_ShouldQuit;
static PLATFORM_ATOMIC Bool8 saWin32_SoundDevice_IsReady;

static PlatformCriticalSection sWin32_SoundQueue_CriticalSection;
static uint32_t sWin32_SoundQueue_Capacity; /* is set right after Invader_Setup and constant from then on */
static PLATFORM_ATOMIC uint32_t saWin32_SoundQueue_Size;
static uint32_t sWin32_SoundQueue_BufferSizeBytes;
static uint8_t *sWin32_SoundQueue_Arena;




void *memset(void *Dst, int Val, size_t SizeBytes)
{
    uint8_t *DstPtr = Dst;
    while (SizeBytes--)
    {
        *DstPtr++ = Val;
    }
    return Dst;
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
        Platform_SwapBuffer(&sWin32_BackBuffer);
    } break;
    default: 
    {
        return DefWindowProcW(Window, Msg, WParam, LParam);
    } break;
    }
    return 0;
}


static void Win32_WaveOutCallback(
    HWAVEOUT WaveOut, 
    UINT Msg, 
    DWORD_PTR Instance, 
    DWORD_PTR Param1, 
    DWORD_PTR Param2)
{
    (void)WaveOut, (void)Instance, (void)Param1, (void)Param2;
    if (WOM_DONE != Msg)
        return;

    PLATFORM_ATOMIC_START(&sWin32_SoundQueue_CriticalSection);
    {
        saWin32_SoundQueue_Size++;
        saWin32_SoundDevice_IsReady = true;
    }
    PLATFORM_ATOMIC_END(&sWin32_SoundQueue_CriticalSection);
}


/* TODO: this code looks very portable, 
 * consider moving this to Invader.c */
static DWORD WINAPI Win32_SoundThreadRoutine(void *UserData)
{
    Win32_SoundThreadData *Data = UserData;
    unsigned QueueIndex = 0;
    double CurrentTime = 0;
    double TimeStep = 1.0 / (double)Data->AudioFormat.SampleRate;

    volatile Bool8 SoundThreadShouldQuit;
    volatile uint32_t SoundQueueSize;
    while (1)
    {
        PLATFORM_ATOMIC_RMW(
            &sWin32_SoundQueue_CriticalSection, 
            SoundQueueSize = saWin32_SoundQueue_Size 
        );
        if (SoundQueueSize == 0)
        {
            volatile Bool8 SoundDeviceIsReady;
            do {
                /* TODO: use cond var or add sleep */
                PLATFORM_ATOMIC_START(&sWin32_SoundQueue_CriticalSection);
                {
                    SoundDeviceIsReady = saWin32_SoundDevice_IsReady;
                    SoundThreadShouldQuit = saWin32_SoundThread_ShouldQuit;
                }
                PLATFORM_ATOMIC_END(&sWin32_SoundQueue_CriticalSection);
            } while (!SoundDeviceIsReady && !SoundThreadShouldQuit);
        }
        PLATFORM_ATOMIC_START(&sWin32_SoundQueue_CriticalSection);
        {
            saWin32_SoundDevice_IsReady = false;
            saWin32_SoundQueue_Size--;
            SoundThreadShouldQuit = saWin32_SoundThread_ShouldQuit;
        }
        PLATFORM_ATOMIC_END(&sWin32_SoundQueue_CriticalSection);


        /* exit if this thread was signaled to do so */
        if (SoundThreadShouldQuit)
            break;


        /* variables to ease typing */
        int16_t *CurrentBuffer = (int16_t *)&Data->Buffers[
            QueueIndex * sWin32_SoundQueue_BufferSizeBytes
        ];
        WAVEHDR *CurrentHeader = &Data->Headers[QueueIndex];

        /* copy sound to buffer */
        for (uint32_t i = 0; 
            i < sWin32_SoundQueue_BufferSizeBytes / sizeof(int16_t); 
            i += Data->AudioFormat.ChannelCount)
        {
            int16_t SoundSample = Invader_OnSoundThreadRequestingSample(CurrentTime, TimeStep);
            for (uint32_t Channel = 0; 
                Channel < Data->AudioFormat.ChannelCount; 
                Channel++)
            {
                CurrentBuffer[i + Channel] = SoundSample;
            }
            CurrentTime += TimeStep;
        }

        /* send the sound buffer to waveOut */
        CurrentHeader->lpData = (LPSTR)CurrentBuffer;
        CurrentHeader->dwBufferLength = sWin32_SoundQueue_BufferSizeBytes;
        if (CurrentHeader->dwFlags & WHDR_PREPARED)
        {
            waveOutUnprepareHeader(sWin32_SoundDevice, CurrentHeader, sizeof *CurrentHeader);
        }
        waveOutPrepareHeader(sWin32_SoundDevice, CurrentHeader, sizeof *CurrentHeader);
        waveOutWrite(sWin32_SoundDevice, CurrentHeader, sizeof *CurrentHeader);


        /* update */
        QueueIndex++;
        /* the capacity of the sound queue should not be changing during the game loop,
         * TODO: assert this */
        QueueIndex %= sWin32_SoundQueue_Capacity; 
    }
    return 0;
}

/* returns error message, or null if there were no error */
static const char *Win32_InitializeAudio(Win32_SoundThreadData *SoundThreadData)
{
    const char *ErrorMessage = NULL;


    /* set audio format */
    PlatformAudioFormat AudioFormat = SoundThreadData->AudioFormat;
    sWin32_AudioFormat = (WAVEFORMATEX) {
        .nChannels = AudioFormat.ChannelCount,
        .wBitsPerSample = 16,
        .nBlockAlign = sizeof(int16_t) * AudioFormat.ChannelCount,
        .wFormatTag = WAVE_FORMAT_PCM,
        .nSamplesPerSec = AudioFormat.SampleRate,
        .nAvgBytesPerSec = 
            sizeof(int16_t) 
            * AudioFormat.ChannelCount 
            * AudioFormat.SampleRate,
    };


    /* init audio device */
    MMRESULT WaveOutErrorCode = waveOutOpen(
        &sWin32_SoundDevice, 
        WAVE_MAPPER, 
        &sWin32_AudioFormat, 
        (DWORD_PTR)&Win32_WaveOutCallback, 
        0, 
        CALLBACK_FUNCTION
    );
    saWin32_SoundDevice_IsReady = MMSYSERR_NOERROR == WaveOutErrorCode;
    if (!saWin32_SoundDevice_IsReady)
    {
        static char ErrorBuffer[MAXERRORLENGTH];
        waveOutGetErrorTextA(WaveOutErrorCode, ErrorBuffer, STATIC_ARRAY_SIZE(ErrorBuffer));
        ErrorMessage = ErrorBuffer;
        goto WaveOutError;
    }


    /* create a critical section for the sound thread */
    sWin32_SoundQueue_CriticalSection = Platform_CreateCriticalSection();
    saWin32_SoundThread_ShouldQuit = false;


    /* allocate sound thread data */
    uint32_t SoundBufferSizeBytes = AudioFormat.QueueSize * AudioFormat.BufferSizeBytes;
    uint32_t SoundHeaderSizeBytes = AudioFormat.QueueSize * sizeof(WAVEHDR);
    sWin32_SoundQueue_Arena = VirtualAlloc(
        NULL, 
        SoundBufferSizeBytes + SoundHeaderSizeBytes, 
        MEM_COMMIT, 
        PAGE_READWRITE
    );
    if (NULL == sWin32_SoundQueue_Arena)
    {
        ErrorMessage = "Out of memory.";
        goto AllocError;
    }


    /* create the sound thread */
    SoundThreadData->Buffers = sWin32_SoundQueue_Arena + 0;
    SoundThreadData->Headers = (WAVEHDR *)(sWin32_SoundQueue_Arena + SoundBufferSizeBytes);
    DWORD ThreadID;
    sWin32_SoundThread_Handle = CreateThread(
        NULL, 
        0, 
        Win32_SoundThreadRoutine, 
        &SoundThreadData, 
        0, 
        &ThreadID
    );
    if (INVALID_HANDLE_VALUE == sWin32_SoundThread_Handle)
    {
        ErrorMessage = "Unable to create audio thread.";
        goto ThreadError;
    }


    /* no error */
    return NULL;


ThreadError:
    /* don't want a buffer lingering around while the game is running */
    VirtualFree(sWin32_SoundQueue_Arena, 0, MEM_RELEASE);
AllocError:
WaveOutError:
    saWin32_SoundDevice_IsReady = false;
    saWin32_SoundThread_ShouldQuit = true;
    return ErrorMessage;
}

static void Win32_DestroyAudio(Win32_SoundThreadData *SoundThreadData)
{
    /* signal the sound thread to stop */
    PLATFORM_ATOMIC_RMW(
        &sWin32_SoundQueue_CriticalSection,
        saWin32_SoundThread_ShouldQuit = true
    );
    /* don't care about return value here since we're exiting anyway */
    (void)WaitForSingleObject(sWin32_SoundThread_Handle, INFINITE);

    /* close the sound thread handle */
    if (INVALID_HANDLE_VALUE != sWin32_SoundThread_Handle)
    {
        CloseHandle(sWin32_SoundThread_Handle);
    }

    /* destroy critical section */
    Platform_DestroyCriticalSection(&sWin32_SoundQueue_CriticalSection);
    
    /* close audio device (waveOut) */
    waveOutClose(sWin32_SoundDevice);

    /* we don't care about the sound arena because 
     * it will get cleaned up by Windows anyway */
    (void)sWin32_SoundQueue_Arena;
    (void)SoundThreadData->Buffers;
    (void)SoundThreadData->Headers;
}

static Bool8 Win32_PollInputs(void)
{
    MSG Msg;
    while (PeekMessageW(&Msg, 0, 0, 0, PM_REMOVE))
    {
        if (Msg.message == WM_QUIT)
            return false;

        TranslateMessage(&Msg);
        DispatchMessageW(&Msg);
    }
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

    sWin32_MainWindow = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WindowClass.lpszClassName,
        L"Space Invader",
        WS_OVERLAPPEDWINDOW | WS_BORDER | WS_CAPTION,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1080, 720,
        NULL, NULL, NULL, NULL
    );
    if (NULL == sWin32_MainWindow)
    {
        Platform_FatalError("Cannot create window");
    }
    ShowWindow(sWin32_MainWindow, SW_SHOW);
    /*
     * gcc didn't link correctly or something??? 
     * uncomment resulting in a bunch of NULLs on the stack 
     * so I couldn't view the backtrace either.
     * Works fine with tcc????
     *
     * UpdateWindow(sWin32_MainWindow);
     * */

    Win32_SoundThreadData SoundThreadData = { 0 };
    Invader_Setup(&SoundThreadData.AudioFormat);
    if (SoundThreadData.AudioFormat.ShouldHaveSound)
    {
        Win32_InitializeAudio(&SoundThreadData);
    }
    while (Win32_PollInputs())
    {
        Invader_Loop();
    }
    Invader_AtExit();

    /* dont't need to cleanup the window, 
     * Windows does it for us */
    (void)sWin32_MainWindow;

    /* but we do need to clean up audio ourselves because it
     * has handles to actual hardware */
    if (SoundThreadData.AudioFormat.ShouldHaveSound)
    {
        Win32_DestroyAudio(&SoundThreadData);
    }
    
    ExitProcess(0);
}




void Platform_Sleep(unsigned Millisec)
{
    Sleep(Millisec);
}

double Platform_GetTimeMillisec(void)
{
    return GetTickCount64();
}


void Platform_SetBackBufferDimension(uint32_t Width, uint32_t Height)
{
    sWin32_BackBuffer.Width = Width;
    sWin32_BackBuffer.Height = Height;

    uint32_t *Buffer = sWin32_BackBuffer.Data;
    if (NULL != Buffer)
    {
        VirtualFree(Buffer, 0, MEM_RELEASE);
    }
    Buffer = VirtualAlloc(
        NULL, 
        Width * Height * sizeof(Buffer[0]), 
        MEM_COMMIT, 
        PAGE_READWRITE
    );
    if (NULL == Buffer)
    {
        Platform_FatalError("Unable to resize window.");
    }
    
    /* memset 0 */
    for (uint32_t i = 0; i < Width*Height; i++)
    {
        Buffer[i] = 0;
    }

    sWin32_BackBuffer.Data = Buffer;
}

PlatformBackBuffer Platform_GetBackBuffer(void)
{
    return sWin32_BackBuffer;
}

void Platform_SwapBuffer(const PlatformBackBuffer *BackBuffer)
{
    /* force Windows to redraw the window */
    InvalidateRect(sWin32_MainWindow, NULL, FALSE);

    PAINTSTRUCT PaintStruct;
    HDC DeviceContext = BeginPaint(sWin32_MainWindow, &PaintStruct);
    RECT Rect = PaintStruct.rcPaint;

    int DstX = Rect.left, 
        DstY = Rect.top,
        DstW = Rect.right - Rect.left,
        DstH = Rect.bottom - Rect.top;
    double ScreenAspectRatio = (double)DstW / DstH;
    double BackBufferAspectRatio = (double)BackBuffer->Width / (double)BackBuffer->Height;
    /* window is wider than game */
    if (ScreenAspectRatio > BackBufferAspectRatio)
    {
        /* scale width to fit */
        DstW = DstH * BackBufferAspectRatio;
        int Middle = (Rect.right - Rect.left) / 2;
        DstX = DstX + Middle - DstW/2;
    }
    else /* game is wider */
    {
        /* scale height to fit */
        DstH = DstW / BackBufferAspectRatio;
        int Middle = (Rect.bottom - Rect.top) / 2;
        DstY = DstY + Middle - DstH/2;
    }

    sWin32_BackBuffer_Bitmap.bmiHeader.biWidth = BackBuffer->Width;
    sWin32_BackBuffer_Bitmap.bmiHeader.biHeight = -BackBuffer->Height; /* reverse, going from top to bottom */
    StretchDIBits(DeviceContext, 
        DstX, DstY, DstW, DstH, 
        0, 0, BackBuffer->Width, BackBuffer->Height, 
        BackBuffer->Data, &sWin32_BackBuffer_Bitmap, 
        DIB_RGB_COLORS, SRCCOPY
    );

    EndPaint(sWin32_MainWindow, &PaintStruct);
}


PlatformCriticalSection Platform_CreateCriticalSection(void)
{
    PlatformCriticalSection Crit;
    InitializeCriticalSection(&Crit.Sect);
    return Crit;
}

void Platform_EnterCriticalSection(PlatformCriticalSection *Crit)
{
    EnterCriticalSection(&Crit->Sect);
}

void Platform_LeaveCriticalSection(PlatformCriticalSection *Crit)
{
    LeaveCriticalSection(&Crit->Sect);
}

void Platform_DestroyCriticalSection(PlatformCriticalSection *Crit)
{
    DeleteCriticalSection(&Crit->Sect);
}


void Platform_FatalError(const char *ErrorMessage)
{
    MessageBoxA(NULL, ErrorMessage, "Fatal Error", MB_ICONERROR);
    ExitProcess(1);
}

void Platform_PrintError(const char *ErrorMessage)
{
    MessageBoxA(NULL, ErrorMessage, "Error", MB_ICONERROR);
}

