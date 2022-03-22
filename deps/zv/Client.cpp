//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "Client.h"

#include "kissnet_zv.h"
#include "Message.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <deque>
#include <cassert>
#include <iostream>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
# define PLATFORM_WINDOWS 1
#endif

#if !PLATFORM_WINDOWS
# include <signal.h>
#endif

using namespace std::chrono_literals;
namespace kn = kissnet;

namespace zv
{

struct ClientPayloadWriter : PayloadWriter
{
    ClientPayloadWriter(std::vector<uint8_t>& payload) : PayloadWriter (payload) {}

    void appendImageBuffer (const ImageView& imageBuffer)
    {
        appendUInt32 (imageBuffer.width);
        appendUInt32 (imageBuffer.height);
        appendUInt32 (imageBuffer.bytesPerRow);
        if (imageBuffer.bytesPerRow > 0)
            appendBytes ((uint8_t*)imageBuffer.pixels_RGBA32, imageBuffer.numBytes());
    }
};

class ClientWriteThread
{
public:
    ClientWriteThread ()
    {
        
    }

    ~ClientWriteThread ()
    {
        stop ();
    }

    void start (kn::tcp_socket* socket)
    {
        _socket = socket;
        _writeThread = std::thread([this]() {
            run ();
        });
    }

    void stop ()
    {
        _messageAdded.notify_all();
        _shouldDisconnect = true;
        if (_writeThread.joinable())
            _writeThread.join();
    }

    void enqueueMessage (Message&& msg)
    {
        std::lock_guard<std::mutex> _(_outputQueueMutex);
        _outputQueue.push_back (std::move(msg));
        _messageAdded.notify_all();
    }

private:
    void run ()
    {
        while (!_shouldDisconnect)
        {
            std::unique_lock<std::mutex> lk (_outputQueueMutex);
            _messageAdded.wait (lk, [this]() {
                return _shouldDisconnect || !_outputQueue.empty();
            });
            // Mutex locked again.

            std::clog << "[DEBUG][WRITER] Got event, checking if anything to send." << std::endl;
            std::deque<Message> messagesToSend;
            messagesToSend.swap(_outputQueue);
            lk.unlock ();

            while (!messagesToSend.empty())
            {
                try
                {
                    sendMessage(*_socket, std::move(messagesToSend.front()));
                    messagesToSend.pop_front();
                }
                catch (const std::exception& e)
                {
                    std::clog << "Got an exception, stopping the connection: " << e.what() << std::endl;
                    _shouldDisconnect = true;
                    break;
                }
            }
        }
    }

private:
    kn::tcp_socket* _socket = nullptr;
    bool _shouldDisconnect = false;
    std::thread _writeThread;
    std::mutex _outputQueueMutex;
    std::deque<Message> _outputQueue;
    std::condition_variable _messageAdded;
};

class MessageImageWriter : public ImageWriter
{
public:
    MessageImageWriter (Message& msg, uint64_t imageId) : msg (msg), writer (msg.payload)
    {
        msg.kind = MessageKind::ImageBuffer;
        writer.appendUInt64 (imageId);
    }

    ~MessageImageWriter ()
    {
        msg.payloadSizeInBytes = msg.payload.size();
    }

    virtual void write (const ImageView& imageView) override
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
    ~ClientThread()
    {
        stop ();
    }

    bool isConnected () const
    {
        return _socket.is_valid();
    }

    void waitUntilDisconnected ()
    {
        if (_thread.joinable())
            _thread.join ();
    }

    bool start(const std::string &hostname, int port)
    {
#if !PLATFORM_WINDOWS
        sigset_t sig_block, sig_restore, sig_pending;
        sigemptyset(&sig_block);
        sigaddset(&sig_block, SIGPIPE);
        if (pthread_sigmask(SIG_BLOCK, &sig_block, &sig_restore) != 0) 
        {
            // Could not block sigmask?
            assert (false);
        }
#endif
        _socket = kn::tcp_socket(kn::endpoint(hostname, port));
        
        try
        {
            _socket.connect ();
        }
        catch (const std::exception& e)
        {
            std::clog << "Could not connect to ZV client: " << e.what() << std::endl;
            return false;
        }

        _thread = std::thread([this]() { runMainLoop(); });
        return true;
    }

    void stop ()
    {
        _shouldDisconnect = true;
        enqueueMessage (closeMessage());
        if (_thread.joinable())
            _thread.join();
    }

    void enqueueMessage (Message&& msg)
    {
        return _writeThread.enqueueMessage (std::move(msg));
    }

    void addImage (uint64_t imageId, const std::string& imageName, const Client::GetDataCallback& getDataCallback, bool replaceExisting)
    {
        if (!_socket.is_valid())
            return;

        {
            std::lock_guard<std::mutex> lk(_getDataCallbacksMutex);
            assert(_getDataCallbacks.find(imageId) == _getDataCallbacks.end());
            _getDataCallbacks[imageId] = getDataCallback;
        }
        addImage(imageId, imageName, ImageView(), replaceExisting);
    }

    void addImage (uint64_t imageId, const std::string& imageName, const ImageView& imageBuffer, bool replaceExisting)
    {
        if (!_socket.is_valid())
            return;

        // uniqueId:uint64_t name:StringUTF8 flags:uint32_t imageBuffer:ImageBuffer
        // ImageBuffer
        //      format: uint32_t
        //      width:uint32_t
        //      height:uint32_t
        //      bytesPerRow:uint32_t
        //      buffer:Blob
        Message msg;
        msg.kind = MessageKind::Image;
        msg.payloadSizeInBytes = (
            sizeof(uint64_t) // imageId
            + imageName.size() + sizeof(uint64_t) // name
            + sizeof(uint32_t) // flags
            + sizeof(uint32_t)*3 + imageBuffer.numBytes() // image buffer
        );
        msg.payload.reserve (msg.payloadSizeInBytes);

        uint32_t flags = replaceExisting;
        ClientPayloadWriter w (msg.payload);
        w.appendUInt64 (imageId);
        w.appendStringUTF8 (imageName);    
        w.appendUInt32 (flags);
        w.appendImageBuffer (imageBuffer);
        assert (msg.payload.size() == msg.payloadSizeInBytes);

        enqueueMessage (std::move(msg));
    }

private: 
    // The main loop will keep reading.
    // The write loop will keep writing.
    void runMainLoop ()
    {
        _writeThread.start (&_socket);
        _writeThread.enqueueMessage (versionMessage(1));

        while (!_shouldDisconnect)
        {       
            try 
            {
                Message msg = recvMessage();
                switch (msg.kind)
                {
                case MessageKind::Invalid:
                {
                    std::clog << "[DEBUG][READER] Invalid message" << std::endl;
                    _shouldDisconnect = true;
                    break;
                }

                case MessageKind::Close:
                {
                    std::clog << "[DEBUG][READER] got close message" << std::endl;
                    if (!_shouldDisconnect)
                    {
                        _writeThread.enqueueMessage (closeMessage ());
                    }
                    _shouldDisconnect = true;
                    break;
                }

                case MessageKind::Version:
                {
                    PayloadReader r (msg.payload);
                    int32_t serverVersion = r.readInt32();
                    std::clog << "[DEBUG][READER] Server version = " << serverVersion << std::endl;
                    assert(serverVersion == 1);
                    break;
                }

                case MessageKind::RequestImageBuffer:
                {
                    // uniqueId:uint64_t
                    PayloadReader r (msg.payload);
                    uint64_t imageId = r.readUInt64();

                    Message outputMessage;
                    {
                        MessageImageWriter msgWriter(outputMessage, imageId);
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
                    _writeThread.enqueueMessage (std::move(outputMessage));
                    break;
                }
                }
            }
            catch (const std::exception& e)
            {
                std::clog << "Got an exception, stopping the connection: " << e.what() << std::endl;
                break;
            }
        }

        _writeThread.stop ();
        _socket.close ();
    }

private:
    Message recvMessage ()
    {
        KnReader r (_socket);
        Message msg;
        msg.kind = (MessageKind)r.recvUInt32 ();
        msg.payloadSizeInBytes = r.recvUInt64 ();
        msg.payload.resize (msg.payloadSizeInBytes);
        if (msg.payloadSizeInBytes > 0)
            r.recvAllBytes (msg.payload.data(), msg.payloadSizeInBytes);
        return msg;
    }

private: 
    std::thread _thread; 
    ClientWriteThread _writeThread;

    kn::tcp_socket _socket;
    bool _shouldDisconnect = false;

    std::mutex _getDataCallbacksMutex;
    std::unordered_map<uint64_t, Client::GetDataCallback> _getDataCallbacks;
};

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

void Client::addImage (uint64_t imageId, const std::string& imageName, const ImageView& imageBuffer, bool replaceExisting)
{
    impl->_clientThread.addImage (imageId, imageName, imageBuffer, replaceExisting);
}

void Client::addImage (uint64_t imageId, const std::string& imageName, const GetDataCallback& getDataCallback, bool replaceExisting)
{
    impl->_clientThread.addImage (imageId, imageName, getDataCallback, replaceExisting);
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
    client.addImage (client.nextUniqueId(), name, ImageView((uint8_t*)pixels_RGBA32, width, height, bytesPerRow));
}

void waitUntilDisconnected ()
{
    Client& client = Client::instance();
    client.waitUntilDisconnected ();
}

} // zv
