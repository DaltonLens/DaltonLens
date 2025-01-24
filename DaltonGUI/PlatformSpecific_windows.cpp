//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

# define NOMINMAX

#include "PlatformSpecific.h"

#include <Dalton/Utils.h>
#include <Dalton/OpenGL.h>

#include "DaltonGeneratedConfig.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

#include <thread>
#include <mutex>
#include <atomic>

namespace dl
{

struct DisplayLinkTimer::Impl
{};


DisplayLinkTimer::DisplayLinkTimer ()
: impl (new Impl())
{
}

DisplayLinkTimer::~DisplayLinkTimer ()
{
    
}

void DisplayLinkTimer::setCallback (const std::function<void(void)>& callback)
{
    dl_assert (false, "not implemented");
}

} // dl

namespace dl
{

struct KeyboardMonitor::Impl
{
    struct Shortcut
    {
        unsigned int modifiers = 0;
        int keycode = -1;
        std::function<void(void)> callback = nullptr;
        std::atomic<int> numPresses = {0};
    };

    Shortcut ctrlCmdAltSpace;
    std::thread hotkeyThread;    
};

KeyboardMonitor::KeyboardMonitor ()
: impl (new Impl())
{   
    impl->hotkeyThread = std::thread ([this]() {
        // See https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey
        bool ok = RegisterHotKey (
            NULL,
            1,
            MOD_ALT | MOD_CONTROL | MOD_WIN | MOD_NOREPEAT,
            VK_SPACE);

        dl_assert (ok, "Could not register the hotkey");

        MSG msg = { 0 };
        while (GetMessage (&msg, NULL, 0, 0) != 0)
        {
            if (msg.message == WM_HOTKEY)
            {
                impl->ctrlCmdAltSpace.numPresses.fetch_add(1);
                dl_dbg ("Hot key pressed!");
            }
        }

        UnregisterHotKey (NULL, 1);
    });   
}

KeyboardMonitor::~KeyboardMonitor ()
{
    PostThreadMessage(GetThreadId(impl->hotkeyThread.native_handle()), WM_QUIT, 0, 0);
    impl->hotkeyThread.join ();
}

// Bool to know if pressed or released.
void KeyboardMonitor::setKeyboardCtrlAltCmdFlagsCallback (const std::function<void(bool)>& callback)
{
    dl_assert (false, "not implemented on Linux");
}

void KeyboardMonitor::setKeyboardCtrlAltCmdSpaceCallback (const std::function<void(void)>& callback)
{
    impl->ctrlCmdAltSpace.callback = callback;
}

void KeyboardMonitor::runOnce ()
{
    const int numPresses = impl->ctrlCmdAltSpace.numPresses.exchange(0);
    if (impl->ctrlCmdAltSpace.callback)
    {
        for (int i = 0; i < numPresses; ++i)
            impl->ctrlCmdAltSpace.callback ();
    }
}

} // dl

namespace dl
{

struct ScreenGrabber::Impl
{
    ~Impl()
    {
        
    }
};
    
ScreenGrabber::ScreenGrabber ()
: impl (new Impl())
{
    
}

ScreenGrabber::~ScreenGrabber ()
{
}

BITMAPINFOHEADER createBitmapHeader(int width, int height)
{
	BITMAPINFOHEADER  bi;

	// create a bitmap
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = width;
	bi.biHeight = -height;  //this is the line that makes it draw upside down or not
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	return bi;
}

/*
 * DXGI allows for direct GPU capture of the frames, but it's meant to capture a sequence of images.
 * There are constraints on the number of apps that use it (4 max), and reinitializing the process
 * will likely take time. So it seems overkill for what we want, which is a single screenshot
 * sporadically.
 * 
 * So for now let's use the good old GDI interface to get the CPU image, and upload it to our GL texture. 
 * Example for DXGI: https://gist.github.com/mmozeiko/80989aa8f46901b2d7a323f3f3165790
 * Example for GDI: https://gist.github.com/SuperKogito/a6383dddcf4ee459b979e12550cc6e51
 */
bool ScreenGrabber::grabScreenArea (const dl::Rect& screenRect, dl::ImageSRGBA& cpuImage, GLTexture& gpuTexture)
{
    HWND hwnd = GetDesktopWindow();

    // get handles to a device context (DC)
	HDC hwindowDC = GetDC(hwnd);
	HDC hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);

	// create a bitmap
	HBITMAP hbwindow = CreateCompatibleBitmap(hwindowDC, screenRect.size.x, screenRect.size.y);
	BITMAPINFOHEADER bi = createBitmapHeader(screenRect.size.x, screenRect.size.y);

	// use the previously created device context with the bitmap
	SelectObject(hwindowCompatibleDC, hbwindow);

	// copy from the window device context to the bitmap device context
    bool ok = StretchBlt (hwindowCompatibleDC,
        0, 0, screenRect.size.x, screenRect.size.y,
        hwindowDC,
        screenRect.origin.x, screenRect.origin.y, screenRect.size.x, screenRect.size.y,
        SRCCOPY);
    if (!ok)
    {
        dl_dbg ("ScreenCapture failed");
        return false;
    }

    // Allocate manually to ensure zero padding.
    cpuImage = ImageSRGBA (
        (uint8_t*)malloc (screenRect.size.x * screenRect.size.y * 4),
        screenRect.size.x,
        screenRect.size.y,
        screenRect.size.x * 4);
	//copy from hwindowCompatibleDC to hbwindow
    GetDIBits(hwindowCompatibleDC, hbwindow, 0, screenRect.size.y, cpuImage.rawBytes(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // Swap red and blue since Windows uses BGRA internally.
    for (int r = 0; r < cpuImage.height(); ++r)
    {
        PixelSRGBA* rowPtr = cpuImage.atRowPtr(r);
        PixelSRGBA* endRow = rowPtr + cpuImage.width();
        while (rowPtr != endRow)
        {
            std::swap (rowPtr->b, rowPtr->r);
            ++rowPtr;
        }
    }

	// avoid memory leak
	DeleteObject(hbwindow);
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(hwnd, hwindowDC);

    gpuTexture.initialize ();
    gpuTexture.upload(cpuImage);

    return true;
}

dl::Rect rectFromRECT (RECT& r)
{
    float adjustedLeft = r.left < 0 ? 0 : r.left;
    float adjustedTop = r.top < 0 ? 0 : r.top;
    return dl::Rect::from_x_y_w_h(adjustedLeft, adjustedTop, r.right - adjustedLeft, r.bottom - adjustedTop);
}

struct WindowUnderPointerFinder
{
    dl::Rect findWindowGeometryUnderPointer (GLFWwindow* grabWindowHandle)
    {        
        _grabWindowHandle = glfwGetWin32Window(grabWindowHandle);

        POINT P;
        GetCursorPos(&P);
        _cursorPos.x = P.x;
        _cursorPos.y = P.y;

        dl_dbg ("_cursorPos = %f %f", _cursorPos.x, _cursorPos.y);

        // Will go over the windows, starting with the front-most one.
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            WindowUnderPointerFinder* that = reinterpret_cast<WindowUnderPointerFinder*>(lParam);
            
            // Skip our own window.
            if (hwnd == that->_grabWindowHandle)
                return true;

            if (!IsWindowVisible(hwnd))
                return true;

            // Check if the cursor is in the window.
            // Note: GetWindowRect will include an invisible border on
            // Windows 10, leading to an extra width.
            // Workarounds seem tricky, so accepting this for now.
            RECT r;
            GetWindowRect(hwnd, &r);
            dl::Rect rect = rectFromRECT(r);
            if (!rect.contains(that->_cursorPos))
                return true;

            that->_frontMostRect = rect;
            return false;
        }, (LPARAM)this);

        dl_dbg ("_frontMostRect = %f %f %f %f", _frontMostRect.origin.x, _frontMostRect.origin.y, _frontMostRect.size.x, _frontMostRect.size.y);
        return _frontMostRect;
    }

    dl::Point _cursorPos;
    HWND _grabWindowHandle = nullptr;
    dl::Rect _frontMostRect;
};

dl::Rect getFrontWindowGeometry(GLFWwindow* grabWindowHandle)
{
    WindowUnderPointerFinder finder;
    return finder.findWindowGeometryUnderPointer (grabWindowHandle);
}

} // dl

namespace dl
{

void setAppFocusEnabled (bool enabled)
{    
    // NSApp.activationPolicy = enabled ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory;
}

dl::Point getMouseCursor()
{
    // NSPoint mousePos = [NSEvent mouseLocation];
    // return dl::Point(mousePos.x, mousePos.y);
    dl_assert (false, "Not implemented");
    return dl::Point(0,0);
}



void setWindowFlagsToAlwaysShowOnActiveDesktop(GLFWwindow* window)
{
    // Display* display = glfwGetX11Display();
    // Window w = glfwGetX11Window (window);
    // xdo_set_desktop_for_window (display, w, -1 /* all desktops */);

    
    // NSWindow* nsWindow = (NSWindow*)glfwGetCocoaWindow(window);
    // dl_assert (nsWindow, "Not working?");
    // nsWindow.collectionBehavior = nsWindow.collectionBehavior | NSWindowCollectionBehaviorMoveToActiveSpace;
}

void openURLInBrowser(const char* url)
{
    // [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]];
    ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

void getVersionAndBuildNumber(std::string& version, std::string& build)
{
    // https://stackoverflow.com/questions/10015304/refer-to-build-number-or-version-number-in-code
    // NSDictionary *infoDict = [[NSBundle mainBundle] infoDictionary];
    // NSString *appVersion = [infoDict objectForKey:@"CFBundleShortVersionString"]; // example: 1.0.0
    // NSString *buildNumber = [infoDict objectForKey:@"CFBundleVersion"]; // example: 42
    // version = [appVersion UTF8String];
    // build = [buildNumber UTF8String];
    version = PROJECT_VERSION;
    build = PROJECT_VERSION_COMMIT;
}

// --------------------------------------------------------------------------------
// StartupManager
// --------------------------------------------------------------------------------

static bool checkInRegistryIfLaunchAtStartupEnabled ()
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExA (HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD dwType;
        LSTATUS nResult = RegQueryValueExA (hKey, "DaltonLens", NULL, &dwType, NULL, NULL);
        bool exists = (nResult == ERROR_SUCCESS);
        RegCloseKey (hKey);
        return exists;
    }

    return false;
}

StartupManager::StartupManager ()
{
    _isEnabled = checkInRegistryIfLaunchAtStartupEnabled ();
}

void StartupManager::setLaunchAtStartupEnabled (bool enabled)
{
    if (enabled == _isEnabled)
        return;

    _isEnabled = enabled;

    HKEY hKey = nullptr;
    HRESULT hres = RegCreateKeyA (HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey);
    if (enabled)
    {
        CHAR exePath[MAX_PATH];
        GetModuleFileNameA (NULL, exePath, MAX_PATH);
        hres = RegSetValueExA (hKey, "DaltonLens", 0, REG_SZ, (BYTE*)exePath, strlen (exePath) + 1);
    }
    else
    {
        RegDeleteValueA (hKey, "DaltonLens");
    }
    RegCloseKey (hKey);
}

} // dl
