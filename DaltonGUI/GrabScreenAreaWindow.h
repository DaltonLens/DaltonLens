#include <memory>
#include <functional>

class GLFWwindow;

namespace dl
{

// Manages a single ImGui window.
class GrabScreenAreaWindow
{
public:
    GrabScreenAreaWindow();
    ~GrabScreenAreaWindow();
    
public:
    bool initialize (GLFWwindow* parentContext);
    void shutdown ();
    void runOnce (float mousePosX, float mousePosY, const std::function<unsigned(void)>& allocateTextureUnderCursor);
    bool isEnabled () const;
    void setEnabled (bool enabled);
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
