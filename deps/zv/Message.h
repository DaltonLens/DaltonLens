//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <vector>

namespace zv
{

/**
    PROTOCOL

    All encodings are expected to be transmitted as little endian.
    Big-endian platforms will need adjustments.

    = Overview =

    Communications are asynchronous and message-based.
    Both the client and server can send and receive messages
    simultaneously.

    = Message Structure =

    Message:
        kind: uint32_t
        contentSizeInBytes: uint64_t
        content: uint8_t[]

    The message content itself will be encoded depending on
    the message kind. See the enum comments in MessageKind.

    = Basic Types =

    StringUTF8: 
        sizeInBytes:uint32_t
        chars:uint8_t[]

    Blob: 
        sizeInBytes:uint64_t
        data:uint8_t[]

    ImageBuffer:
        format: uint32_t
        width:uint32_t
        height:uint32_t
        bytesPerRow:uint32_t
        buffer:Blob
*/

enum class MessageKind : int32_t
{
    Invalid = -1,

    // No payload. Just request a close message.
    Close = 0,

    // version:uint32_t
    Version = 1,

    // Sending a new image. If the ImageContent has a zero width and height then
    // the server knows it'll need to request the data with GetImageData when
    // it'll need it. This is useful when telling the server about many
    // available images (e.g listing a folder). 
    //
    // uniqueId:uint64_t name:StringUTF8 flags:uint32_t imageBuffer:ImageBuffer
    Image = 2,

    // Request image data.
    // uniqueId:uint64_t
    RequestImageBuffer = 3, // the server needs the data for the given image name.

    // Output image data.
    // uniqueIdInClient:uint64_t imageBuffer:ImageBuffer
    ImageBuffer = 4,
};

struct Message
{
    Message () {}

    // Make sure we don't accidentally make copies by leaving
    // only the move operators.
    Message (Message&& msg) = default;
    Message& operator= (Message&& msg) = default;

    MessageKind kind = MessageKind::Invalid;
    uint64_t payloadSizeInBytes = 0;
    std::vector<uint8_t> payload;
};

struct PayloadWriter
{
    PayloadWriter (std::vector<uint8_t>& payload) : payload(payload) {}
    
    template <class T>
    void appendValue (const T& value)
    {
        const uint8_t* valuePtr = reinterpret_cast<const uint8_t*>(&value);
        std::copy (valuePtr, valuePtr + sizeof(value), std::back_inserter(payload));
    }

    void appendInt32 (int32_t value) { appendValue(value); }
    void appendUInt32 (uint32_t value) { appendValue(value); }
    void appendUInt64 (uint64_t value) { appendValue(value); }

    void appendBytes (const uint8_t* bytes, size_t numBytes) { std::copy (bytes, bytes + numBytes, std::back_inserter(payload)); }
    void appendStringUTF8 (const std::string& s) { appendBlob (reinterpret_cast<const uint8_t*>(s.c_str()), s.size()); }

    // Bytes + a size.
    void appendBlob (const uint8_t* bytes, size_t numBytes)
    {
        appendUInt64(numBytes);
        appendBytes (bytes, numBytes);
    }

    std::vector<uint8_t>& payload;
};

struct PayloadReader
{
    PayloadReader (const std::vector<uint8_t>& payload) : payload(payload) {}

    template <class T>
    T readValue ()
    {
        T v;
        uint8_t* outputPtr = reinterpret_cast<uint8_t*>(&v);
        if (payload.size() < offset + sizeof(T))
        {
            throw std::overflow_error("Could not read an expected value.");
        }
        std::copy (payload.begin() + offset, payload.begin() + offset + sizeof(T), outputPtr);
        offset += sizeof(T);
        return v;
    }

    int32_t readInt32() { return readValue<int32_t>(); }
    uint32_t readUInt32() { return readValue<uint32_t>(); }
    uint64_t readUInt64() { return readValue<uint64_t>(); }

    void readBytes (uint8_t* outputPtr, size_t numBytes)
    {
        std::copy(payload.begin() + offset, payload.begin() + offset + numBytes, outputPtr);
        offset += numBytes;
    }

    void skipBytes (size_t numBytes)
    {
        offset += numBytes;
    }

    void readBlob(std::vector<uint8_t> &outputData)
    {
        size_t numBytes = readUInt64();

        if (payload.size() < offset + numBytes)
        {
            throw std::overflow_error("Could not read an expected blob.");
        }

        outputData.resize(numBytes);
        readBytes (outputData.data(), numBytes);
    }

    void readStringUTF8(std::string& s)
    {
        size_t numBytes = readUInt64();

        if (payload.size() < offset + numBytes)
        {
            throw std::overflow_error("Could not read an expected string.");
        }

        s.assign((const char *)payload.data() + offset, numBytes);
        offset += numBytes;
    }

private:
    const std::vector<uint8_t>& payload;
    size_t offset = 0;
};

static Message closeMessage()
{
    Message msg;
    msg.kind = MessageKind::Close;
    msg.payloadSizeInBytes = 0;
    return msg;
}

static Message versionMessage(int32_t version)
{
    Message msg;
    msg.kind = MessageKind::Version;
    msg.payloadSizeInBytes = sizeof(uint32_t);
    msg.payload.reserve(msg.payloadSizeInBytes);
    PayloadWriter w (msg.payload);
    w.appendInt32 (version);
    assert (msg.payload.size() == msg.payloadSizeInBytes);
    return msg;
}

} // zv