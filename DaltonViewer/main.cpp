#include "DaltonViewerLib.h"

int main(int argc, char** argv)
{
    DaltonViewer viewer;
    viewer.initialize(argc, argv);
    
    // Main loop
    while (!viewer.shouldExit())
    {
        viewer.runOnce();
    }

    return 0;
}
