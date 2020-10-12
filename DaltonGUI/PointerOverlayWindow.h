#include <memory>
#include <functional>

class GLFWwindow;

namespace dl
{

// Manages a single ImGui window.
class PointerOverlayWindow
{
public:
    PointerOverlayWindow();
    ~PointerOverlayWindow();
    
public:
    bool initialize (GLFWwindow* parentContext);
    void shutdown ();
    
    bool isEnabled () const;
    void setEnabled (bool enabled);
    
    void runOnce ();
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
