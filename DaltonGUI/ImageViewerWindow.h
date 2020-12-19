#include <memory>
#include <functional>

class GLFWwindow;

namespace dl
{

struct GrabScreenData;

// Manages a single ImGuiWindow
class ImageViewerWindow
{
public:
    ImageViewerWindow();
    ~ImageViewerWindow();
    
public:
    bool initialize (int argc, char** argv, GLFWwindow* parentWindow);
    bool initialize (GLFWwindow* parentWindow);
    
    void showGrabbedData (const GrabScreenData& grabbedData);
    
    void shutdown ();
    void runOnce ();
    bool shouldExit () const;
    
    bool isEnabled () const;
    void dismiss ();
    
    bool helpWindowRequested () const;
    void notifyHelpWindowRequestHandled ();
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
