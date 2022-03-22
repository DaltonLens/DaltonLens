//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include "kissnet.hpp"

#include "Message.h"

#include <vector>

namespace kn = kissnet;

namespace zv
{

struct connection_closed_exception : public std::exception
{
	const char * what () const throw ()
    {
    	return "Connection was closed.";
    }
};

class KnReader
{
public:
    KnReader(kn::tcp_socket& s) : s(s) {}

    void recvAllBytes(uint8_t *output_bytes, size_t numBytes)
    {
        std::byte *bytes = reinterpret_cast<std::byte *>(output_bytes);
        size_t bytesRead = 0;
        while (bytesRead < numBytes)
        {
            auto [received_bytes, status] = s.recv(bytes + bytesRead, numBytes - bytesRead);
            if (!status)
                throw connection_closed_exception();
            bytesRead += received_bytes;
        }
    }

    template <class T>
    T recvValue()
    {
        T value;
        uint8_t *bytes = reinterpret_cast<uint8_t *>(&value);
        recvAllBytes(bytes, sizeof(T));
        return value;
    }

    uint32_t recvInt32() { return recvValue<int32_t>(); }
    uint32_t recvUInt32() { return recvValue<uint32_t>(); }
    uint64_t recvUInt64() { return recvValue<uint64_t>(); }    

private:
    kn::tcp_socket& s;    
};

static Message recvMessage (kn::tcp_socket& s)
{
    KnReader r(s);
    Message msg;
    msg.kind = (MessageKind)r.recvUInt32();
    msg.payloadSizeInBytes = r.recvUInt64();
    msg.payload.resize(msg.payloadSizeInBytes);
    if (msg.payloadSizeInBytes > 0)
        r.recvAllBytes(msg.payload.data(), msg.payloadSizeInBytes);
    return msg;
}

class KnWriter
{
public:
    KnWriter(kn::tcp_socket& s) : s(s) {}

    // Make sure that all the bytes got sent.
    void sendAllBytes(const uint8_t *input_bytes, size_t numBytes)
    {
        const std::byte *bytes = reinterpret_cast<const std::byte *>(input_bytes);
        size_t bytesSent = 0;
        while (bytesSent < numBytes)
        {
            auto [sent_bytes, status] = s.send(bytes + bytesSent, numBytes - bytesSent);
            if (!status)
                throw connection_closed_exception();
            bytesSent += sent_bytes;
        }
    }

    template <class T>
    void sendValue(const T &value) { sendAllBytes(reinterpret_cast<const uint8_t *>(&value), sizeof(T)); }

    void sendInt32(int32_t value) { sendValue(value); }
    void sendUInt32(uint32_t value) { sendValue(value); }
    void sendUInt64(uint64_t value) { sendValue(value); }

private:
    kn::tcp_socket& s;
};

static void sendMessage (kn::tcp_socket& s, const Message& msg)
{
    KnWriter w (s);
    w.sendInt32 ((int32_t)msg.kind);
    w.sendUInt64 (msg.payloadSizeInBytes);
    if (msg.payloadSizeInBytes > 0)
        w.sendAllBytes (msg.payload.data(), msg.payloadSizeInBytes);
}

} // zv