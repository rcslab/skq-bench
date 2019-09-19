#ifndef IOGEN_IOINSTR_H
#define IOGEN_IOINSTR_H

// TODO : Document

enum class Opcode { READ, WRITE };

class IOInstr {
private:
    unsigned long size;
    char * buff;
    Opcode opcode;
public:
    IOInstr(unsigned long size, Opcode opcode);

    unsigned long getSize() const;

    Opcode getOpcode() const;

    char * getBuff() const;
};


#endif //IOGEN_IOINSTR_H
