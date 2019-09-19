#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <vector>
#include "randomio.h"

using namespace std;

// TODO : Document

void
test()
{
    cout << "IOGEN_TEST:" << endl;
    RandomIO randomIO = RandomIO(Dist::UNIFORM, "sddfdsfsdfdf", 0.3, { 3, 5 });
    auto instructions = randomIO.gen(5);
    for (auto instr : instructions) {
        if (instr.getOpcode() == Opcode::READ) std::cout << "READ" << std::endl;
        if (instr.getOpcode() == Opcode::WRITE) std::cout << "WRITE" << std::endl;
        std::cout << instr.getSize() << std::endl;
        std::cout << instr.getBuff() << std::endl;
    }
}

int
main()
{
    test();
    return 0;
}
