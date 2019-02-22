#include <iostream>

#include "src/resp_machine.h"

int main() {
    RespMachine machine;
    const char s[] = "EXISTS my_key\r\n";
    machine.Input(s, strlen(s));
    for (const auto & arg : machine.GetArgv()) {
        std::cout << arg << std::endl;
    }
    std::cout << "Done." << std::endl;
    return 0;
}