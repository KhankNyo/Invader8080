
#include <windows.h>
#include "Invaders.c"
#include "Platform.h"


#define METHOD_CALL(pObj, Meth) (pObj)->lpVtbl->Meth
#define DESTRUCT(pObj) if (NULL != (pObj)) METHOD_CALL(pObj, Release)(pObj)
#define WIN32_FN_DIRECT_SOUND_CREATE(name)\
    HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter)
#define SOUND_QUEUE_INITVAL 255


static PlatformBackBuffer sWin32_BackBuffer = { 0 };

static HWND sMainWindow;
static WAVEFORMATEX sDefaultAudioFormat = {
    .nChannels = 2,     /* stereo */
    .wFormatTag = WAVE_FORMAT_PCM,
    .wBitsPerSample = 16,
    .nBlockAlign = 2 * sizeof(int16_t),
    .nSamplesPerSec = 44100,
    .nAvgBytesPerSec = 44100 * (2 * sizeof(int16_t)), 
};
static BITMAPINFO sBitmapInfo = {
    .bmiHeader.biSize = sizeof(sBitmapInfo.bmiHeader),
    .bmiHeader.biWidth = 0,
    .bmiHeader.biHeight = 0, 
    .bmiHeader.biPlanes = 1,
    .bmiHeader.biBitCount = 32,
    .bmiHeader.biCompression = BI_RGB,
};
static HWAVEOUT sSoundDevice;
static Bool8 sSoundDeviceIsReady;
static WAVEHDR sBlocks[256] = { 0 };
static uint8_t sTail = SOUND_QUEUE_INITVAL;
static uint8_t sHead = SOUND_QUEUE_INITVAL;
static unsigned sQueueSize = 16;


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
        waveOutUnprepareHeader(sSoundDevice, Header, sizeof *Header);
    waveOutPrepareHeader(sSoundDevice, Header, sizeof *Header);
    waveOutWrite(sSoundDevice, Header, sizeof *Header);
}


static unsigned Win32_SoundDeviceQueueSize(void)
{
    int Val = sTail - sHead;
    if (sHead > sTail)
        Val += STATIC_ARRAY_SIZE(sBlocks);
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
    if (QueueSize < sQueueSize)
    {
        sSoundDeviceIsReady = true;
        if (QueueSize == 0)
        {
            sTail = SOUND_QUEUE_INITVAL;
            sHead = SOUND_QUEUE_INITVAL;
        }
        else sTail--;
    }
    else
    {
        sSoundDeviceIsReady = false;
        sTail--;
        uint8_t Index = sTail - sQueueSize;
        Win32_WriteWaveHeaderToSoundDevice(&sBlocks[Index]);
    }
}

Bool8 Platform_SoundDeviceIsReady(void)
{
    return sSoundDeviceIsReady;
}

void Platform_WriteToSoundDevice(const void *SoundBuffer, size_t SoundBufferSize)
{
    if (sHead == sTail)
    {
        sHead = SOUND_QUEUE_INITVAL;
        sTail = SOUND_QUEUE_INITVAL;
    }

    /* push the new block into the queue */
    WAVEHDR *NewBlock = &sBlocks[sHead--];
    *NewBlock = (WAVEHDR){
        .lpData = (LPSTR)SoundBuffer,
        .dwBufferLength = SoundBufferSize,
    };

    if (Win32_SoundDeviceQueueSize() < sQueueSize)
    {
        Win32_WriteWaveHeaderToSoundDevice(NewBlock);
    }
    else sSoundDeviceIsReady = false; 
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
        Platform_FatalError("Cannot create window");
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

    sSoundDeviceIsReady = MMSYSERR_NOERROR == waveOutOpen(
        &sSoundDevice, 
        WAVE_MAPPER, 
        &sDefaultAudioFormat, 
        (DWORD_PTR)&Win32_WaveOutCallback, 
        0, 
        CALLBACK_FUNCTION
    );
    if (!sSoundDeviceIsReady)
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
    InvalidateRect(sMainWindow, NULL, FALSE);

    PAINTSTRUCT PaintStruct;
    HDC DeviceContext = BeginPaint(sMainWindow, &PaintStruct);
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

    sBitmapInfo.bmiHeader.biWidth = BackBuffer->Width;
    sBitmapInfo.bmiHeader.biHeight = -BackBuffer->Height; /* reverse, buffer going from top to bottom */
    StretchDIBits(DeviceContext, 
        DstX, DstY, DstW, DstH, 
        0, 0, BackBuffer->Width, BackBuffer->Height, 
        BackBuffer->Data, &sBitmapInfo, 
        DIB_RGB_COLORS, SRCCOPY
    );

    EndPaint(sMainWindow, &PaintStruct);
}






void Platform_Exit(int ExitCode)
{
    ExitProcess(ExitCode);
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

