/*
 * Celestis::BufferedStream
 * Copyright (c) 2005-2018 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <string>
#include <algorithm>
#include <stdexcept>

#include <celestis/debug.h>
#include <celestis/stream.h>
#include <celestis/bufferedstream.h>

namespace Celestis
{

BufferedStream::BufferedStream()
{
    inputBufferLength = 0;
    inputBufferCur = 0;
    inputBuffer = nullptr;
    outputBufferLength = 0;
    outputBufferCur = 0;
    outputBuffer = nullptr;
    io = nullptr;
}

BufferedStream::BufferedStream(const BufferedStream &b)
{
}

BufferedStream::BufferedStream(Stream *ios,
                               int inputbuflen,
                               int outputbuflen)
{
    inputBufferLength = inputbuflen;
    inputBufferCur = 0;
    inputBuffer = new char[inputBufferLength];

    outputBufferLength = outputbuflen;
    outputBufferCur = 0;
    outputBuffer = new char[outputBufferLength];

    io = ios;
}

BufferedStream::~BufferedStream()
{
    if (inputBuffer != nullptr) {
        delete inputBuffer;
    }

    if (outputBuffer != nullptr) {
        delete outputBuffer;
    }
}

void
BufferedStream::close()
{
    flush();
    io->close();
    return;
}

void
BufferedStream::flush()
{
    io->write(outputBuffer, 0, outputBufferCur);
    inputBufferCur = 0;
    outputBufferCur = 0;

    io->flush();
    return;
}

size_t
BufferedStream::read(char *buf, off_t offset, size_t count)
{
    size_t matchLength = 0;
    size_t copyLength = 0;

    if (offset == 0) {
        matchLength = std::min(count, inputBufferCur);
        memcpy(buf, inputBuffer, matchLength);
        memcpy(inputBuffer, inputBuffer + matchLength,
               inputBufferCur - matchLength);
        copyLength += matchLength;
        inputBufferCur -= matchLength;
        buf += matchLength;
        count -= matchLength;
    } else {
        io->write(outputBuffer, 0, outputBufferCur);
        outputBufferCur = 0;
        io->seek(offset - inputBufferCur, SEEK_CUR);
        inputBufferCur = 0;
    }

    while (copyLength != count) {
        if (count < inputBufferLength) {
            matchLength = io->read(inputBuffer, 0, inputBufferLength);
            memcpy(buf,inputBuffer,count);
            memcpy(inputBuffer,
		   inputBuffer + count,
		   matchLength - count);
            inputBufferCur = matchLength - count;
        } else {
            matchLength = io->read(buf, 0, count);
        }
        copyLength += matchLength;
        buf += matchLength;
        count -= matchLength;
    }

    return copyLength;
}

char
BufferedStream::readByte()
{
    char retval;

    if (inputBufferCur > 0) 
    {
        retval = inputBuffer[0];
        memcpy(inputBuffer, inputBuffer + 1, inputBufferCur - 1);
    } else {
        inputBufferCur = io->read(inputBuffer, 0, inputBufferLength);
        retval = inputBuffer[0];
        memcpy(inputBuffer, inputBuffer + 1, inputBufferCur - 1);
    }
    inputBufferCur--;

    return retval;
}

size_t
BufferedStream::readLine(char *buf, size_t bufLength)
{
    size_t i;

    for (i = 0; i < bufLength; i++)
    {
        *buf = readByte();
        if (*buf == '\n')
            break;

        buf++;
    }

    *buf = '\0';

    return i;
}

std::string
BufferedStream::readLine(size_t maxLength)
{
    char buf[maxLength];

    readLine(buf, sizeof(buf));

    return buf;
}

off_t
BufferedStream::seek(off_t offset, int origin)
{
    flush();

    return io->seek(offset, origin);
}

void
BufferedStream::setLength(size_t length)
{
    flush();

    io->setLength(length);
}

void
BufferedStream::write(const char *buf, off_t offset, size_t count)
{
    int matchLength = 0;

    if (offset != 0 || outputBufferCur == outputBufferLength) {
        io->flush();
    }

    // XXX: No support for offsets
    ASSERT(offset == 0);

    matchLength = std::min(count, outputBufferLength - outputBufferCur);
    memcpy(outputBuffer + outputBufferCur, buf, matchLength);
    buf += matchLength;
    count -= matchLength;
    outputBufferCur += matchLength;

    if (count > 0) {
        io->write(outputBuffer, 0, outputBufferCur);
        inputBufferCur = 0;
        outputBufferCur = 0;

        if (count > outputBufferLength)
        {
            io->write(buf, 0, count);
        } else {
            write(buf, 0, count);
        }
    }
}

void
BufferedStream::writeByte(char value)
{
    if (outputBufferCur == outputBufferLength)
        flush();

    outputBuffer[outputBufferCur] = value;
    outputBufferCur++;

    return;
}

void
BufferedStream::writeLine(const std::string &str)
{
    write(str.c_str(), 0, str.size());
}

bool
BufferedStream::canRead()
{
    return io->canRead();
}

bool
BufferedStream::canSeek()
{
    return io->canSeek();
}

bool
BufferedStream::canWrite()
{
    return io->canWrite();
}

}
