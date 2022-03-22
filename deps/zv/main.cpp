#include "Client.h"

#include <thread>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

int main (int argc, char** argv)
{
    zv::Client client;
    if (!client.connect ("127.0.0.1", 4207))
    {
        return 1;
    }

    int w = 128;
    int h = 64;
    std::vector<uint8_t> imData (128*64*4);
    zv::ImageView view (imData.data(), 128, 64);

    for (int i = 0; i < 5; ++i)
    {
        std::fill (imData.begin(), imData.end(), i*16);
        std::string name = "TestImage-" + std::to_string(argc) + "-" + std::to_string(i);
        client.addImage (i, name, view);
        std::this_thread::sleep_for(100ms);
    }

    // uint64_t imageId, const std::string& imageName, const GetDataCallback& getDataCallback, bool replaceExisting = true
    std::string filename = "/home/nb/Perso/zv/tests/rgbgrid.png";
    client.addImage (5, "withCallback", [filename](zv::ImageWriter& writer) {
        std::clog << "Image " << filename << " requested" << std::endl;
        std::vector<uint8_t> imData (256*256*4);
        std::fill (imData.begin(), imData.end(), 127);
        zv::ImageView view (imData.data(), 256, 256);
        writer.write (view);
        return true;
    });

    while (client.isConnected())
    {
        std::this_thread::sleep_for(1000ms);
    }

    return 0;
}
