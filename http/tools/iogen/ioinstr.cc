#include "ioinstr.h"

#include <random>

IOInstr::IOInstr(unsigned long size, Opcode opcode) : size(size), opcode(opcode)
{
    std::random_device random_device;
    std::uniform_int_distribution<int> distribution(32, 124);

    this->buff = static_cast<char *>(malloc(sizeof(char) * size));

    for (int i=0; i < size; i++) {
        this->buff[i] = static_cast<char>(distribution(random_device));
    }
}

Opcode IOInstr::getOpcode() const
{
    return this->opcode;
}

unsigned long IOInstr::getSize() const
{
    return this->size;
}

char *IOInstr::getBuff() const
{
    return this->buff;
}


