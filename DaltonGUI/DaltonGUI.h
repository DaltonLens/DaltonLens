#include <memory>
#include <functional>

namespace dl
{

// Manages a single ImGuiWindow
class ImageViewer
{
public:
    ImageViewer();
    ~ImageViewer();
    
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

// Manages a single ImGui window.
class DaltonGrabScreen
{
public:
    DaltonGrabScreen();
    ~DaltonGrabScreen();
    
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



} // dl
