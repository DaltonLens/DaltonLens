//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include "Message.h"

#include <functional>
#include <memory>
#include <string>

namespace zv
{

// View of an existing buffer, no ownership will be taken and no reference
// will get stored.
struct ClientImageBuffer
{
    ClientImageBuffer() {}

    ClientImageBuffer(void* pixels_RGBA32, int width, int height, int bytesPerRow = 0)
    :   format(ImageBufferFormat::Data_RGBA32), 
        data(pixels_RGBA32), 
        width(width), 
        height(height), 
        bytesPerRow(bytesPerRow)
    {
        if (this->bytesPerRow == 0)
            this->bytesPerRow = width * 4;
    }

    ClientImageBuffer(const std::string& filePath, void* fileContent, size_t fileLength)
    :   format(ImageBufferFormat::Raw_File),
        filePath (filePath),
        data (fileContent),
        width (0), 
        height(1),
        bytesPerRow (fileLength)
    {
    }

    inline size_t contentSizeInBytes() const { return height * bytesPerRow; }

    ImageBufferFormat format = ImageBufferFormat::Empty;

    std::string filePath;
    void* data = nullptr;
    int width = 0;
    int height = 0;
    int bytesPerRow = 0;
};

inline size_t messagePayloadSize (const ClientImageBuffer& im)
{
    return 4*sizeof(uint32_t) + im.contentSizeInBytes() + im.filePath.size() + sizeof(uint64_t);
}

class ClientImageWriter
{
public:
    virtual void write (const ClientImageBuffer& imageView) = 0;
};

class Client
{
public:
    Client ();
    ~Client ();

public:
    // We don't strictly need it to be a singleton, but it's convenient
    // for the logImage utility.
    static Client& instance();

public:
    static uint64_t nextUniqueId ();

    bool isConnected () const;
    bool connect (const std::string& hostname = "127.0.0.1", int port = 4207);
    void waitUntilDisconnected ();
    void disconnect ();

    using GetDataCallback = std::function<bool(ClientImageWriter&)>;
    
    void addImage (uint64_t imageId, const std::string& imageName, const ClientImageBuffer& imageBuffer, bool replaceExisting = true, const std::string& viewerName = "default");
    void addImage (uint64_t imageId, const std::string& prettyName, const std::string& fileName, const GetDataCallback& getDataCallback, bool replaceExisting = true, const std::string& viewerName = "default");

    void addImageFromFile (uint64_t imageId, const std::string& imagePath);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

bool connect (const std::string& hostname = "127.0.0.1", int port = 4207);
void logImageRGBA (const std::string& name, void* pixels_RGBA32, int width, int height, int bytesPerRow = 0);
void waitUntilDisconnected ();

} // zv
