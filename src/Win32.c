#include <windows.h>
#include "Invaders.c"
#include "Platform.h"




#define METHOD_CALL(pObj, Meth) (pObj)->lpVtbl->Meth
#define DESTRUCT(pObj) if (NULL != (pObj)) METHOD_CALL(pObj, Release)(pObj)
#define WIN32_FN_DIRECT_SOUND_CREATE(name)\
    HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter)
#define SOUND_QUEUE_INITVAL 255




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

static WAVEFORMATEX sWin32_DefaultAudioFormat = {
    .nChannels = 2,     /* stereo */
    .wFormatTag = WAVE_FORMAT_PCM,
    .wBitsPerSample = 16,
    .nBlockAlign = 2 * sizeof(int16_t),
    .nSamplesPerSec = 44100,
    .nAvgBytesPerSec = 44100 * (2 * sizeof(int16_t)), 
};
static HWAVEOUT sWin32_SoundDevice;
static Bool8 sWin32_SoundDeviceIsReady;

static WAVEHDR sWin32_SoundQueue[256] = { 0 };
static uint8_t sWin32_SoundQueue_Tail = SOUND_QUEUE_INITVAL;
static uint8_t sWin32_SoundQueue_Head = SOUND_QUEUE_INITVAL;
static unsigned sWin32_SoundQueue_Size = 16;




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

static void Win32_WriteWaveHeaderToSoundDevice(WAVEHDR *Header)
{
    if (Header->dwFlags & WHDR_PREPARED)
        waveOutUnprepareHeader(sWin32_SoundDevice, Header, sizeof *Header);
    waveOutPrepareHeader(sWin32_SoundDevice, Header, sizeof *Header);
    waveOutWrite(sWin32_SoundDevice, Header, sizeof *Header);
}

static unsigned Win32_SoundDeviceQueueSize(void)
{
    int Val = sWin32_SoundQueue_Tail - sWin32_SoundQueue_Head;
    if (sWin32_SoundQueue_Head > sWin32_SoundQueue_Tail)
        Val += STATIC_ARRAY_SIZE(sWin32_SoundQueue);
    return (unsigned)ABS(Val);
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

    unsigned QueueSize = Win32_SoundDeviceQueueSize();
    if (QueueSize < sWin32_SoundQueue_Size)
    {
        sWin32_SoundDeviceIsReady = true;
        if (QueueSize == 0)
        {
            sWin32_SoundQueue_Tail = SOUND_QUEUE_INITVAL;
            sWin32_SoundQueue_Head = SOUND_QUEUE_INITVAL;
        }
        else sWin32_SoundQueue_Tail--;
    }
    else
    {
        sWin32_SoundDeviceIsReady = false;
        sWin32_SoundQueue_Tail--;
        uint8_t Index = sWin32_SoundQueue_Tail - sWin32_SoundQueue_Size;
        Win32_WriteWaveHeaderToSoundDevice(&sWin32_SoundQueue[Index]);
    }
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

    sWin32_SoundDeviceIsReady = MMSYSERR_NOERROR == waveOutOpen(
        &sWin32_SoundDevice, 
        WAVE_MAPPER, 
        &sWin32_DefaultAudioFormat, 
        (DWORD_PTR)&Win32_WaveOutCallback, 
        0, 
        CALLBACK_FUNCTION
    );
    if (!sWin32_SoundDeviceIsReady)
    {
        Platform_PrintError("Unable to play sound (waveOutOpen failed).");
    }

    Invader_Setup();
    while (Win32_PollInputs())
    {
        Invader_Loop();
    }
    /* dont't need to cleanup the windows, Windows does it for us */
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


Bool8 Platform_SoundDeviceIsReady(void)
{
    return sWin32_SoundDeviceIsReady;
}

void Platform_WriteToSoundDevice(const void *SoundBuffer, size_t SoundBufferSize)
{
    if (sWin32_SoundQueue_Head == sWin32_SoundQueue_Tail)
    {
        sWin32_SoundQueue_Head = SOUND_QUEUE_INITVAL;
        sWin32_SoundQueue_Tail = SOUND_QUEUE_INITVAL;
    }

    /* push the new block into the queue */
    WAVEHDR *NewBlock = &sWin32_SoundQueue[sWin32_SoundQueue_Head--];
    *NewBlock = (WAVEHDR){
        .lpData = (LPSTR)SoundBuffer,
        .dwBufferLength = SoundBufferSize,
    };

    if (Win32_SoundDeviceQueueSize() < sWin32_SoundQueue_Size)
    {
        Win32_WriteWaveHeaderToSoundDevice(NewBlock);
    }
    else sWin32_SoundDeviceIsReady = false; 
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

