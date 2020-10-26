#include <Dalton/Image.h>
#include <Dalton/MathUtils.h>
#include "Graphics.h"

#include "imgui.h"

#include <memory>
#include <functional>

class GLFWwindow;

namespace dl
{

struct GrabScreenData
{
    bool isValid = false;
    dl::Rect capturedScreenRect;
    std::shared_ptr<dl::ImageSRGBA> srgbaImage;
    std::shared_ptr<GLTexture> texture;
};

void showImageCursorOverlayTooptip (const dl::ImageSRGBA& image,
                                    GLTexture& imageTexture,
                                    ImVec2 imageWidgetTopLeft,
                                    ImVec2 imageWidgetSize,
                                    const ImVec2& uvTopLeft = ImVec2(0,0),
                                    const ImVec2& uvBottomRight = ImVec2(1,1),
                                    const ImVec2& roiWindowSize = ImVec2(15,15));

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
