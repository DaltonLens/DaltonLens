#include "DaltonGUI/DaltonGUI.h"

int main(int argc, char** argv)
{
    ImageViewer viewer;
    if (!viewer.initialize(argc, argv))
        return 1;
    
    // Main loop
    while (!viewer.shouldExit())
    {
        viewer.runOnce();
    }

    return 0;
}
