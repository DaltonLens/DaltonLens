#include <memory>
#include <functional>

#if 0

// CrossPlatform APIs

void grabScreenArea (const dl::Rect& gpuRect, const dl::Rect& cpuRect, dl::Image& cpuArea, GLTexture& gpuArea);

struct DisplayLinkTimer
{
    void setCallback (std::function<void(void)>);
    void setEnabled ();
};

struct KeyboardMonitor
{
    void setKeyboardCtrlAltCmdFlagsCallback (bool keysDown);
    void setKeyboardCtrlAltCmdSpaceCallback ();
};

dl::Point getMouseCursor();

class DaltonLens
{
public:
    // Call it once per app, calls glfwInit, etc.
    // Sets up the callback on keyboard flags
    // Sets up the hotkeys
    void initialize ();

private:
    enum CurrentMode
    {
        GlobalCursorOverlay,
        ViewerWindow,
        GrabScreen,
    };
    
private:
    void onKeyboardFlagsChanged ();
    void onHotkeyPressed ();
    void onDisplayLinkUpdated ();
    
private:
    DaltonViewer viewer;
    DaltonPointerOverlay overlay;
    DaltonGrabScreen grabScreen;
};
#endif


// Manages a single ImGuiWindow
class DaltonViewer
{
public:
    DaltonViewer();
    ~DaltonViewer();
    
public:
    bool initialize (int argc, char** argv);
    void runOnce ();
    bool shouldExit () const;
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

// Manages a single ImGui window.
class DaltonPointerOverlay
{
public:
    DaltonPointerOverlay();
    ~DaltonPointerOverlay();
    
public:
    bool initialize ();
    void runOnce (float mousePosX, float mousePosY, const std::function<unsigned(void)>& allocateTextureUnderCursor);
    bool isEnabled () const;
    void setEnabled (bool enabled);
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};
