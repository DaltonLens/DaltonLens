#include "DaltonGUI/DaltonViewerLib.h"

int main(int argc, char** argv)
{
    DaltonViewer viewer;
    if (!viewer.initialize(argc, argv))
        return 1;
    
    // Main loop
    while (!viewer.shouldExit())
    {
        viewer.runOnce();
    }

    return 0;
}
