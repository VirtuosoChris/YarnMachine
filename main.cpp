#include "yarn.h"

#include <iostream>
#include <fstream>

int main(void)
{
    std::clog << "kdfjkdjf" << std::endl;

    YarnMachine m;

    std::ifstream file("./Output.yarnc", std::ios::binary | std::ios::in);

    bool fileOpen = file.is_open();

    m.loadProgram(file);

    m.printNodes();

    auto it = m.program.nodes().begin();
    if (( it = m.program.nodes().find("Start")) != m.program.nodes().end())
    {
        const Yarn::Node& startnode = it->second;

        m.loadNode("Start");
    }

    const Yarn::Instruction& inst = m.programState.currentInstruction();
    m.processInstruction(inst);

    while (m.programState.runningState == YarnMachine::ProgramState::RUNNING)
    {
        const Yarn::Instruction& inst = m.programState.advance();
        m.processInstruction(inst);
    }
}

