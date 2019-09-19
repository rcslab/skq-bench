
#include <cstddef>

#include <string>
#include <sstream>

#include <celestis/rpc/message.h>
#include <celestis/rpc/serializationexception.h>

namespace Celestis { namespace RPC {

Message::Message()
    : buf(new uint8_t[DEFAULT_SIZE]), bufSz(DEFAULT_SIZE), msgSz(8), off(8)
{
    for (uint32_t i = 0; i < msgSz; i++)
	buf[i] = 0;
}

Message::~Message()
{
}

void
Message::clear()
{
    msgSz = 8;
    off = 8;

    // Clear header
    for (uint32_t i = 0; i < msgSz; i++)
	buf[i] = 0;
}

void
Message::reserve(size_t size)
{
    if (size < bufSz)
	return;

    std::unique_ptr<uint8_t[]> newBuf(new uint8_t[size]);

    buf = std::move(newBuf);
    bufSz = size;
    msgSz = 8;
    off = 8;
}

void
Message::seal()
{
    // Magic
    buf[0] = 'X';
    buf[1] = 'R';
    buf[2] = 'P';
    buf[3] = 'C';
    // Size
    buf[4] = msgSz;
    buf[5] = msgSz >> 8;
    // Flags
    buf[6] = 0;
    buf[7] = 0;
}

void
Message::unseal()
{
    uint32_t sz;

    if (buf[0] != 'X' || buf[1] != 'R' ||
	buf[2] != 'P' || buf[3] != 'C')
	throw SerializationException("Bad magic");

    sz = buf[4];
    sz |= buf[5] << 8;

    if (sz != msgSz)
	throw SerializationException("Bad size");

    off = 8;
}

uint32_t
Message::prepareBuffer()
{
    uint32_t sz;

    msgSz = 8;
    off = 8;

    sz = buf[4];
    sz |= buf[5] << 8;
    resize(sz);

    return sz;
}

uint8_t*
Message::data() const
{
    return buf.get();
}

uint32_t
Message::size() const
{
    return msgSz;
}

std::string
Message::dump() const
{
    std::stringstream str;

    // XXX: Dump

    return str.str();
}

// Appending Data

void
Message::appendU8(uint8_t d)
{
    if (off + 2 > msgSz)
	resize(off + 2);

    buf[off] = MSG_TYPE_UINT8;
    buf[off+1] = d;
    off += 2;
}

void
Message::appendU16(uint16_t d)
{
    if (off + 3 > msgSz)
	resize(off + 3);

    buf[off] = MSG_TYPE_UINT16;
    buf[off+1] = d;
    buf[off+2] = d >> 8;
    off += 3;
}

void
Message::appendU32(uint32_t d)
{
    if (off + 5 > msgSz)
	resize(off + 5);

    buf[off] = MSG_TYPE_UINT32;
    buf[off+1] = d;
    buf[off+2] = d >> 8;
    buf[off+3] = d >> 16;
    buf[off+4] = d >> 24;
    off += 5;
}

void
Message::appendU64(uint64_t d)
{
    if (off + 9 > msgSz)
	resize(off + 9);

    buf[off] = MSG_TYPE_UINT64;
    buf[off+1] = d;
    buf[off+2] = d >> 8;
    buf[off+3] = d >> 16;
    buf[off+4] = d >> 24;
    buf[off+5] = d >> 32;
    buf[off+6] = d >> 40;
    buf[off+7] = d >> 48;
    buf[off+8] = d >> 56;
    off += 9;
}

void
Message::appendS8(int8_t d)
{
    if (off + 2 > msgSz)
	resize(off + 2);

    buf[off] = MSG_TYPE_INT8;
    buf[off+1] = d;
    off += 2;
}

void
Message::appendS16(int16_t d)
{
    if (off + 3 > msgSz)
	resize(off + 3);

    buf[off] = MSG_TYPE_INT16;
    buf[off+1] = d;
    buf[off+2] = d >> 8;
    off += 3;
}

void
Message::appendS32(int32_t d)
{
    if (off + 5 > msgSz)
	resize(off + 5);

    buf[off] = MSG_TYPE_INT32;
    buf[off+1] = d;
    buf[off+2] = d >> 8;
    buf[off+3] = d >> 16;
    buf[off+4] = d >> 24;
    off += 5;
}

void
Message::appendS64(int64_t d)
{
    if (off + 9 > msgSz)
	resize(off + 9);

    buf[off] = MSG_TYPE_INT64;
    buf[off+1] = d;
    buf[off+2] = d >> 8;
    buf[off+3] = d >> 16;
    buf[off+4] = d >> 24;
    buf[off+5] = d >> 32;
    buf[off+6] = d >> 40;
    buf[off+7] = d >> 48;
    buf[off+8] = d >> 56;
    off += 9;
}

void
Message::appendStr(const std::string &str)
{
    if (off + str.size() + 4 > msgSz)
	resize(off + str.size() + 4);

    buf[off] = MSG_TYPE_STR;
    buf[off+1] = str.size();
    buf[off+2] = str.size() >> 8;
    for (size_t i = 0; i < str.size(); i++) {
	buf[off+3+i] = str[i];
    }
    buf[off+3+str.size()] = 0;
    off += 4 + str.size();
}

// Reading Data

uint8_t
Message::readU8()
{
    uint8_t d;

    if (off + 2 > msgSz)
	throw SerializationException("Attempted to read past message end");
    if (buf[off] != MSG_TYPE_UINT8)
	throw SerializationException("Unexpected field type");

    d = buf[off+1];
    off += 2;

    return d;
}

uint16_t
Message::readU16()
{
    uint16_t d = 0;

    if (off + 3 > msgSz)
	throw SerializationException("Attempted to read past message end");
    if (buf[off] != MSG_TYPE_UINT16)
	throw SerializationException("Unexpected field type");

    d = buf[off+1];
    d |= buf[off+2] << 8;
    off += 3;

    return d;
}

uint32_t
Message::readU32()
{
    uint32_t d;

    if (off + 5 > msgSz)
	throw SerializationException("Attempted to read past message end");
    if (buf[off] != MSG_TYPE_UINT32)
	throw SerializationException("Unexpected field type");

    d = buf[off+1];
    d |= buf[off+2] << 8;
    d |= buf[off+3] << 16;
    d |= buf[off+4] << 24;
    off += 5;

    return d;
}

uint64_t
Message::readU64()
{
    uint64_t d;

    if (off + 5 > msgSz)
	throw SerializationException("Attempted to read past message end");
    if (buf[off] != MSG_TYPE_UINT64)
	throw SerializationException("Unexpected field type");

    d = (uint64_t)buf[off+1];
    d |= (uint64_t)buf[off+2] << 8;
    d |= (uint64_t)buf[off+3] << 16;
    d |= (uint64_t)buf[off+4] << 24;
    d |= (uint64_t)buf[off+5] << 32;
    d |= (uint64_t)buf[off+6] << 40;
    d |= (uint64_t)buf[off+7] << 48;
    d |= (uint64_t)buf[off+8] << 56;
    off += 9;

    return d;
}

int8_t
Message::readS8()
{
    int8_t d;

    if (off + 2 > msgSz)
	throw SerializationException("Attempted to read past message end");
    if (buf[off] != MSG_TYPE_INT8)
	throw SerializationException("Unexpected field type");

    d = buf[off+1];
    off += 2;

    return d;
}

int16_t
Message::readS16()
{
    int16_t d = 0;

    if (off + 3 > msgSz)
	throw SerializationException("Attempted to read past message end");
    if (buf[off] != MSG_TYPE_INT16)
	throw SerializationException("Unexpected field type");

    d = buf[off+1];
    d |= buf[off+2] << 8;
    off += 3;

    return d;
}

int32_t
Message::readS32()
{
    int32_t d;

    if (off + 5 > msgSz)
	throw SerializationException("Attempted to read past message end");
    if (buf[off] != MSG_TYPE_INT32)
	throw SerializationException("Unexpected field type");

    d = buf[off+1];
    d |= buf[off+2] << 8;
    d |= buf[off+3] << 16;
    d |= buf[off+4] << 24;
    off += 5;

    return d;
}

int64_t
Message::readS64()
{
    int64_t d;

    if (off + 9 > msgSz)
	throw SerializationException("Attempted to read past message end");
    if (buf[off] != MSG_TYPE_INT64)
	throw SerializationException("Unexpected field type");

    d = (int64_t)buf[off+1];
    d |= (int64_t)buf[off+2] << 8;
    d |= (int64_t)buf[off+3] << 16;
    d |= (int64_t)buf[off+4] << 24;
    d |= (int64_t)buf[off+5] << 32;
    d |= (int64_t)buf[off+6] << 40;
    d |= (int64_t)buf[off+7] << 48;
    d |= (int64_t)buf[off+8] << 56;
    off += 9;

    return d;
}

std::string
Message::readStr()
{
    uint16_t sz;
    std::string str;

    if (off + 3 > msgSz)
	throw SerializationException("Attempted to read past message end");
    if (buf[off] != MSG_TYPE_STR)
	throw SerializationException("Unexpected field type");

    sz = buf[off+1];
    sz |= buf[off+2] << 8;

    str.resize(sz);
    for (size_t i = 0; i < sz; i++) {
	str[i] = buf[off+3+i];
    }
    buf[off+3+str.size()] = 0;
    off += 4 + sz;

    return str;
}

Message::Types
Message::peekType()
{
    switch (buf[off])
    {
	case MSG_TYPE_UINT8:
	case MSG_TYPE_INT8:
	case MSG_TYPE_UINT16:
	case MSG_TYPE_INT16:
	case MSG_TYPE_UINT32:
	case MSG_TYPE_INT32:
	case MSG_TYPE_UINT64:
	case MSG_TYPE_INT64:
	case MSG_TYPE_STR:
	case MSG_TYPE_BIN:
	    return (Message::Types)buf[off];
	default:
	    throw SerializationException("Unsupported type!");
    };
}

uint32_t
Message::peekSize()
{
    switch (buf[off])
    {
	case MSG_TYPE_UINT8:
	case MSG_TYPE_INT8:
	    return 2;
	case MSG_TYPE_UINT16:
	case MSG_TYPE_INT16:
	    return 3;
	case MSG_TYPE_UINT32:
	case MSG_TYPE_INT32:
	    return 5;
	case MSG_TYPE_UINT64:
	case MSG_TYPE_INT64:
	    return 9;
	case MSG_TYPE_STR:
	case MSG_TYPE_BIN: {
	    // XXX: Check length
	    uint16_t len = buf[off+1] + (buf[off+2] << 8);
	    return len + 4;
	}
	default:
	    throw SerializationException("Unsupported type!");
    };
}

void
Message::resize(size_t size)
{
    if (size < bufSz) {
	msgSz = size;
	return;
    }

    // Round up to power of 2
    bufSz = size - 1;
    bufSz |= bufSz >> 1;
    bufSz |= bufSz >> 2;
    bufSz |= bufSz >> 4;
    bufSz |= bufSz >> 8;
    bufSz |= bufSz >> 16;
    bufSz = bufSz + 1;

    std::unique_ptr<uint8_t[]> newBuf(new uint8_t[bufSz]);

    for (uint32_t i = 0; i < msgSz; i++)
	newBuf[i] = buf[i];

    buf = std::move(newBuf);
    msgSz = size;
}

}; };

