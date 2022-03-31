//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "znet_zv.h"

#include "Client.h"

#include "Message.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <deque>
#include <cassert>
#include <iostream>
#include <fstream>
#include <atomic>
#include <filesystem>

#include <netdb.h>
#include <arpa/inet.h>

namespace fs = std::filesystem;
namespace zn = zsummer::network;

namespace zv
{

struct ClientPayloadWriter : PayloadWriter
{
    ClientPayloadWriter(std::vector<uint8_t>& payload) : PayloadWriter (payload) {}

    void appendImageBuffer (const ClientImageBuffer& imageBuffer)
    {
        appendUInt32 ((uint32_t)imageBuffer.format);
        appendStringUTF8 (imageBuffer.filePath);
        appendUInt32 (imageBuffer.width);
        appendUInt32 (imageBuffer.height);
        appendUInt32 (imageBuffer.bytesPerRow);
        if (imageBuffer.bytesPerRow > 0)
            appendBytes ((uint8_t*)imageBuffer.data, imageBuffer.contentSizeInBytes());
    }
};

class MessageImageViewWriter : public ClientImageWriter
{
public:
    MessageImageViewWriter (Message& msg, uint64_t imageId) : msg (msg), writer (msg.payload)
    {
        msg.header.kind = MessageKind::ImageBuffer;
        writer.appendUInt64 (imageId);
    }

    ~MessageImageViewWriter ()
    {
        msg.header.payloadSizeInBytes = msg.payload.size();
    }

    virtual void write (const ClientImageBuffer& imageView) override
    {
        writer.appendImageBuffer (imageView);
    }

private:
    Message& msg;
    ClientPayloadWriter writer;
};

class ClientThread
{
public:
    enum class Status
    {
        Init,
        Connecting,
        Connected,
        FailedToConnect,
        Disconnected,
    };

public:
    ~ClientThread()
    {
        stop ();
    }

    bool isConnected () const
    {
        std::lock_guard<std::mutex> _ (_statusMutex);
        return _status == Status::Connected;
    }

    void waitUntilDisconnected ()
    {
        if (_thread.joinable())
            _thread.join ();
    }

    bool start(const std::string& hostname, int port)
    {
        _status = Status::Connecting;
        _thread = std::thread([this, hostname, port]() { runMainLoop(hostname, port); });
        
        Status connectionStatus;
        {
            std::unique_lock<std::mutex> lk(_statusMutex);
            _statusChanged.wait(lk, [this]()
                                { return _status != Status::Connecting; });
            connectionStatus = _status;
        }

        if (connectionStatus != Status::Connected && _thread.joinable())
            _thread.join();

        return connectionStatus == Status::Connected;
    }

    void stop ()
    {
        if (_thread.joinable())
        {
            {
                std::lock_guard<std::mutex> lk(_eventLoopMutex);
                if (_eventLoop)
                    _eventLoop->post([this]() { disconnect(); });
            }
            _thread.join();
        }
    }

    void addImage (uint64_t imageId, const std::string& imageName, const std::string& imagePath, const Client::GetDataCallback& getDataCallback, bool replaceExisting, const std::string& viewerName)
    {
        if (!isConnected())
            return;

        {
            std::lock_guard<std::mutex> lk(_getDataCallbacksMutex);
            assert(_getDataCallbacks.find(imageId) == _getDataCallbacks.end());
            _getDataCallbacks[imageId] = getDataCallback;
        }
        
        // Only write the path.
        ClientImageBuffer imageBuffer;
        imageBuffer.filePath = imagePath;

        addImage(imageId, imageName, imageBuffer, replaceExisting, viewerName);
    }

    void addImage (uint64_t imageId, const std::string& imageName, const ClientImageBuffer& imageBuffer, bool replaceExisting, const std::string& viewerName)
    {
        if (!isConnected())
            return;

        // uniqueId:uint64_t name:StringUTF8 flags:uint32_t imageBuffer:ImageBuffer
        // ImageBuffer
        //      format: uint32_t
        //      width:uint32_t
        //      height:uint32_t
        //      bytesPerRow:uint32_t
        //      buffer:Blob
        Message msg;
        msg.header.kind = MessageKind::Image;
        msg.header.payloadSizeInBytes = (
            sizeof(uint64_t) // imageId
            + imageName.size() + sizeof(uint64_t) // name
            + viewerName.size() + sizeof(uint64_t) // viewerName
            + sizeof(uint32_t) // flags
            + messagePayloadSize(imageBuffer)
        );
        msg.payload.reserve (msg.header.payloadSizeInBytes);

        uint32_t flags = replaceExisting;
        ClientPayloadWriter w (msg.payload);
        w.appendUInt64 (imageId);
        w.appendStringUTF8 (imageName);
        w.appendStringUTF8 (viewerName);
        w.appendUInt32 (flags);
        w.appendImageBuffer (imageBuffer);
        assert (msg.payload.size() == msg.header.payloadSizeInBytes);

        _senderQueue->enqueueMessage (std::move(msg));
    }

private: 
    void disconnect ()
    {
        // Already disconnected.
        if (!_socket)
            return;

        _shouldDisconnect = true;
        _receiver.reset ();
        _senderQueue.reset ();
        _socket->doClose ();
        _socket.reset ();
        setStatus (Status::Disconnected);
    }

    void setStatus (Status status)
    {
        std::lock_guard<std::mutex> _ (_statusMutex);
        _status = status;
        _statusChanged.notify_all ();
    }

    void onMessage(zn::NetErrorCode error, const Message& msg)
    {        
        if (error != zn::NEC_SUCCESS)
            return disconnect();

        switch (msg.header.kind)
        {
        case MessageKind::Invalid:
        {
            std::clog << "[DEBUG][READER] Invalid message" << std::endl;
            disconnect ();
            break;
        }

        case MessageKind::Version:
        {
            PayloadReader r(msg.payload);
            int32_t serverVersion = r.readInt32();
            // std::clog << "[DEBUG][READER] Server version = " << serverVersion << std::endl;
            assert(serverVersion == 1);
            break;
        }

        case MessageKind::RequestImageBuffer:
        {
            // uniqueId:uint64_t
            PayloadReader r(msg.payload);
            uint64_t imageId = r.readUInt64();

            Message outputMessage;
            {
                MessageImageViewWriter msgWriter(outputMessage, imageId);
                Client::GetDataCallback callback;
                {
                    std::lock_guard<std::mutex> lk(_getDataCallbacksMutex);
                    auto callbackIt = _getDataCallbacks.find(imageId);
                    assert(callbackIt != _getDataCallbacks.end());
                    callback = callbackIt->second;
                }

                if (callback)
                {
                    callback(msgWriter);
                }
            }
            _senderQueue->enqueueMessage(std::move(outputMessage));
            break;
        }

        default:
            break;
        }

        // Keep reading.
        recvMessage();
    }

    // https://stackoverflow.com/questions/9400756/ip-address-from-host-name-in-windows-socket-programming
    std::string hostnameToIP(const std::string& host_or_ip)
    {
        hostent *hostname = gethostbyname(host_or_ip.c_str());
        if (hostname)
            return std::string(inet_ntoa(**(in_addr **)hostname->h_addr_list));
        else
            return host_or_ip;
    }

    // The main loop will keep reading.
    // The write loop will keep writing.
    void runMainLoop (const std::string &hostname, int port)
    {        
        _eventLoop = std::make_shared<zn::EventLoop>();
        _eventLoop->initialize ();
        _socket = std::make_shared<zn::TcpSocket>();
        bool ok = _socket->initialize (_eventLoop);
        if (!ok)
        {
            setStatus (Status::FailedToConnect);
            std::clog << "Could not initialize a socket." << std::endl;
            return;
        }

        const std::string ip = hostnameToIP (hostname);

        ok = _socket->doConnect (ip, port, [this, hostname, ip, port](zn::NetErrorCode error) {
            if (error != zn::NEC_SUCCESS)
            {
                fprintf (stderr, "Could not connect to the ZV server %s(%s):%d .\n", hostname.c_str(), ip.c_str(), port);
                setStatus (Status::FailedToConnect);
                return disconnect ();
            }

            // Start the receive message loop.
            _receiver = std::make_shared<zn::MessageReceiver>(_socket);
            recvMessage ();

            _senderQueue = std::make_shared<zn::MessageSenderQueue>(_eventLoop, _socket, [this](zn::NetErrorCode err) {
                if (err != zn::NEC_SUCCESS)
                    disconnect ();
            });

            _senderQueue->enqueueMessage (versionMessage(1));

            setStatus (Status::Connected);
        });
        if (!ok)
        {
            std::clog << "Could not connect to the ZV client." << std::endl;
            return;
        }

        while (!_shouldDisconnect)
        {
            // onMessage will be called once read.
            bool success = _eventLoop->runOnce ();
            if (!success)
                disconnect ();
        }
        
        disconnect ();
        
        {
            std::lock_guard<std::mutex> lk(_eventLoopMutex);
            _eventLoop.reset();
        }
    }

    void recvMessage ()
    {
        if (!_receiver)
            return;
        _receiver->recvMessage([this](zn::NetErrorCode err, const Message &msg) { onMessage(err, msg); });
    }

private: 
    std::thread _thread; 
    
    std::mutex _eventLoopMutex;
    zn::EventLoopPtr _eventLoop;

    zn::TcpSocketPtr _socket;
    zn::MessageReceiverPtr _receiver;
    zn::MessageSenderQueuePtr _senderQueue;

    bool _shouldDisconnect = false;
    Status _status = Status::Init;
    std::condition_variable _statusChanged;
    mutable std::mutex _statusMutex;

    std::mutex _outputQueueMutex;
    std::deque<Message> _outputQueue;

    std::mutex _getDataCallbacksMutex;
    std::unordered_map<uint64_t, Client::GetDataCallback> _getDataCallbacks;
};

} // zv

namespace zv
{

struct Client::Impl
{
    ClientThread _clientThread;
};

Client::Client() : impl (new Impl())
{    
}

Client::~Client() = default;

bool Client::connect (const std::string& hostname, int port)
{
    return impl->_clientThread.start (hostname, port); 
}

bool Client::isConnected () const
{
    return impl->_clientThread.isConnected ();
}

void Client::waitUntilDisconnected ()
{
    impl->_clientThread.waitUntilDisconnected ();
}

void Client::disconnect ()
{
    impl->_clientThread.stop ();
}

void Client::addImage (uint64_t imageId, const std::string& imageName, const ClientImageBuffer& imageBuffer, bool replaceExisting, const std::string& viewerName)
{
    impl->_clientThread.addImage (imageId, imageName, imageBuffer, replaceExisting, viewerName);
}

void Client::addImage (uint64_t imageId, const std::string& imageName, const std::string& fileName, const GetDataCallback& getDataCallback, bool replaceExisting, const std::string& viewerName)
{
    impl->_clientThread.addImage (imageId, imageName, fileName, getDataCallback, replaceExisting, viewerName);
}

inline size_t fileLength (std::ifstream& is)
{
    is.seekg (0, is.end);
    size_t length = is.tellg();
    is.seekg (0, is.beg);
    return length;
}

void Client::addImageFromFile (uint64_t imageId, const std::string& imPath)
{
    auto cb = [imPath](ClientImageWriter& writer) {
        fprintf (stderr, "%s requested", imPath.c_str());
        std::ifstream f(imPath, std::ios::in | std::ios::binary);
        if (!f.good())
            return false;
        // Only accurate on all platforms in binary mode.
        // https://stackoverflow.com/questions/2409504/using-c-filestreams-fstream-how-can-you-determine-the-size-of-a-file
        // https://stackoverflow.com/questions/22984956/tellg-function-give-wrong-size-of-file/22986486#22986486
        size_t sizeInBytes = fileLength (f);
        std::vector<uint8_t> contents (sizeInBytes);
        f.read((char*)contents.data(), sizeInBytes);

        ClientImageBuffer buffer (imPath, contents.data(), sizeInBytes);        
        writer.write (buffer);
        return true;
    };

    addImage (imageId, fs::path(imPath).filename(), imPath, std::move(cb), true);
}

Client& Client::instance()
{
    static std::unique_ptr<Client> client = std::make_unique<Client>();
    return *client.get();
}

uint64_t Client::nextUniqueId ()
{
    static uint64_t nextId = 1;
    return nextId++;
}

bool connect (const std::string& hostname, int port)
{
    Client& client = Client::instance();
    return client.connect (hostname, port);
}

void logImageRGBA (const std::string& name, void* pixels_RGBA32, int width, int height, int bytesPerRow)
{
    Client& client = Client::instance();
    client.addImage (client.nextUniqueId(), name, ClientImageBuffer((uint8_t*)pixels_RGBA32, width, height, bytesPerRow));
}

void waitUntilDisconnected ()
{
    Client& client = Client::instance();
    client.waitUntilDisconnected ();
}

} // zv
