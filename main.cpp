#include <iostream>

#include "src/resp_machine.h"

int main() {
    RespMachine machine;
    {
        const char s[] = "EXISTS somekey\r\n";
        machine.Input(s, strlen(s));
        for (const auto & arg : machine.GetArgv()) {
            std::cout << arg << std::endl;
        }
        machine.Reset();
    }
    {
        const char s[] = "*2\r\n$4\r\nLLEN\r\n$6\r\nmylist\r\n";
        machine.Input(s, strlen(s));
        for (const auto & arg : machine.GetArgv()) {
            std::cout << arg << std::endl;
        }
    }
    std::cout << "Done." << std::endl;
    return 0;
}