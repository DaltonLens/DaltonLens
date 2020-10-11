#include <memory>
#include <functional>

class GLFWwindow;

namespace dl
{

// Manages a single ImGuiWindow
class ImageViewerWindow
{
public:
    ImageViewerWindow();
    ~ImageViewerWindow();
    
public:
    bool initialize (int argc, char** argv, GLFWwindow* parentWindow);
    void shutdown ();
    void runOnce ();
    bool shouldExit () const;
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
