#include <memory>
#include <functional>

class GLFWwindow;

namespace dl
{

class HelpWindow
{
public:
    HelpWindow();
    ~HelpWindow();
    
public:
    bool initialize (GLFWwindow* parentWindow);
    void shutdown ();
    void runOnce ();

    void setEnabled (bool enabled);
    bool isEnabled () const;
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
