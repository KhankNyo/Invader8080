
#include <windows.h>
#include <dsound.h>
#include "Invaders.c"



#define METHOD_CALL(pObj, Meth) (pObj)->lpVtbl->Meth
#define DESTRUCT(pObj) if (NULL != (pObj)) METHOD_CALL(pObj, Release)(pObj)

static uint8_t sBackBuffer[256*224 * 4];
static HWND sMainWindow;
static LPDIRECTSOUNDBUFFER sSecondaryBuffer;

#define WIN32_FN_DIRECT_SOUND_CREATE(name)\
    HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter)


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


static int Win32_InitDirectSound(HWND Window, uint32_t bufsize, uint32_t SamplePerSec)
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
        .dwBufferBytes = bufsize, 
        .lpwfxFormat = &WaveFormat,
    };
    if (FAILED(METHOD_CALL(DSound, CreateSoundBuffer)(DSound, &BufferDescription, &sSecondaryBuffer, 0)))
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

    if (!Win32_InitDirectSound(sMainWindow, 48000, 48000 * (sizeof(uint16_t)*2)))
    {
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




void Platform_Exit(int ExitCode)
{
    ExitProcess(ExitCode);
}

void Platform_PrintError(const char *ErrorMessage)
{
    MessageBoxA(NULL, ErrorMessage, "Error", MB_ICONERROR);
}

