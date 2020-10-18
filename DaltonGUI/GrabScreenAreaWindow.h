#include <Dalton/Image.h>
#include <Dalton/MathUtils.h>
#include "Graphics.h"

#include <memory>
#include <functional>

class GLFWwindow;

namespace dl
{

struct GrabScreenData
{
    bool isValid = false;
    dl::Rect capturedRect;
    std::shared_ptr<dl::ImageSRGBA> srgbaImage;
    std::shared_ptr<GLTexture> texture;
};

// Manages a single ImGui window.
class GrabScreenAreaWindow
{
public:
    GrabScreenAreaWindow();
    ~GrabScreenAreaWindow();
    
public:
    bool initialize (GLFWwindow* parentContext);
    void shutdown ();
    void runOnce ();
    bool startGrabbing ();
    
    bool grabbingFinished() const;
    const GrabScreenData& grabbedData () const;
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
