#include <memory>

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
