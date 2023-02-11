#pragma once

#include <functional>
#include <stack>
#include <iostream>

#include <yarn_spinner.pb.h>

#define YarnException std::runtime_error

//#define CALLBACK_ARGS_PROTOTYPE void
//#define CALLBACK_ARGS NULL
#define NIL_CALLBACK [](){}

struct YarnMachine
{
    /*
    struct StaticContext
    {
        ~StaticContext()
        {
            google::protobuf::ShutdownProtobufLibrary();
        }
    };*/

    struct Line
    {
        std::string id;
        std::vector<Yarn::Operand> substitutions;
    };

    struct Option
    {
        Line line;
        std::string destination; // destination to go to if this option is selected
        bool  enabled;           // whether the option has a condition on it(in which case a value should be popped off the stack and used to signal the game that the option should be not available) (????????????)
    };

    typedef std::function<Yarn::Operand(YarnMachine& yarn, int parameterCount)> YarnFunction; ///\todo arguments
    typedef std::vector<Option> OptionsList;
    OptionsList currentOptionsList;
    std::stack <Yarn::Operand> variableStack;
    std::unordered_map<std::string, Yarn::Operand> variableStorage;
    std::unordered_map<std::string, YarnFunction> functions;
    Yarn::Program program;

    typedef std::function<void(void)> YarnCallback;

#define YARN_CALLBACK(name) YarnCallback name = NIL_CALLBACK;

    struct YarnCallbacks
    {
        YARN_CALLBACK(onProgramStopped);
        YARN_CALLBACK(onChangeNode);

        std::function<void(const YarnMachine::Line&)> onLine = [](const YarnMachine::Line& line) {};
        std::function<void(const std::string&)> onCommand = [](const std::string&) {};
        std::function<void(const OptionsList&)> onShowOptions = [](const OptionsList&) {};
    };

    YarnCallbacks callbacks;

    void selectOption(const Option& option)
    {
        programState.runningState = ProgramState::RUNNING;

        // --- TODO NYI --- 
        // 
        // push some shit onto the stack

        std::int32_t translatedLabel = programState.currentNode->labels().at(option.destination);

        currentOptionsList.resize(0);

        programState.setInstruction(translatedLabel);
    }

    void selectOption(int selection)
    {
        if (selection >= currentOptionsList.size())
        {
            throw YarnException("Invalid option selected");
        }

        selectOption(currentOptionsList[selection]);
    }


    std::string get_string_operand(const Yarn::Instruction& instruction, int index)
    {
        if (instruction.operands_size() <= index)
        {
            throw YarnException("get_string_operand() : Operand out of bounds");
        }

        const Yarn::Operand& op = instruction.operands(index);

        if (!op.has_string_value())
        {
            throw YarnException("get_string_operand() : operand doesn't have a string value");
        }

        return op.string_value();
    }

    bool get_bool_operand(const Yarn::Instruction& instruction, int index)
    {
        if (instruction.operands_size() <= index)
        {
            throw YarnException("get_bool_operand() : Operand out of bounds");
        }

        const Yarn::Operand& op = instruction.operands(index);

        if (!op.has_bool_value())
        {
            throw YarnException("get_bool_operand() : operand doesn't have a bool value");
        }

        return op.bool_value();
    }

    float get_float_operand(const Yarn::Instruction& instruction, int index)
    {
        if (instruction.operands_size() <= index)
        {
            throw YarnException("get_float_operand() : Operand out of bounds");
        }

        const Yarn::Operand& op = instruction.operands(index);

        if (!op.has_float_value())
        {
            throw YarnException("get_float_operand() : operand doesn't have a float value");
        }

        return op.float_value();
    }


    void processInstruction(const Yarn::Instruction& instruction)
    {
        if (!programState.currentNode)
        {
            throw YarnException("Current node is nullptr in processInstruction()");
        }

        //bool jumpInstruction = false;
        auto op = instruction.opcode();

        switch (op)
        {
            case Yarn::Instruction_OpCode_JUMP_TO:
            {
                const std::string& jumpLabel = get_string_operand(instruction, 0);

                // -- hope this syntax is right for this lib!
                if (programState.currentNode->labels().find(jumpLabel) == programState.currentNode->labels().end())
                {
                    throw YarnException("JUMP_TO : Jump label doesn't exist in current node");
                }

                std::int32_t translatedLabel = programState.currentNode->labels().at(jumpLabel);

                programState.setInstruction(translatedLabel);

                return;
            }
            break;
            case Yarn::Instruction_OpCode_JUMP:
            {
                const Yarn::Operand& stackTop = variableStack.top();

                if ((variableStack.size() == 0) || !stackTop.has_string_value())
                {
                    throw YarnException("Stack top doesn't have a string value in JUMP instruction");
                }

                const std::string& jumpLoc = stackTop.string_value();

                std::int32_t translatedLabel = programState.currentNode->labels().at(jumpLoc);

                programState.setInstruction(translatedLabel);

                return;
            }
            break;
            case Yarn::Instruction_OpCode_RUN_LINE:
            {
                //std::cout << "run_line with operand ct : " << instruction.operands_size() << std::endl;
                const std::string& lineID = get_string_operand(instruction, 0);
                std::cout << get_string_operand(instruction, 0) << std::endl;
                std::cout << get_float_operand(instruction, 1) << std::endl;

                int substitutions = get_float_operand(instruction, 1);

                YarnMachine::Line l;
                l.id = lineID;

                l.substitutions.reserve(substitutions);

                for (int i = 0; i < substitutions; i++)
                {
                    l.substitutions.push_back(variableStack.top());
                    variableStack.pop();
                }

                callbacks.onLine(l);
            }
            break;
            case Yarn::Instruction_OpCode_RUN_COMMAND:
            {
                const std::string& commandText = get_string_operand(instruction, 0);
                callbacks.onCommand(commandText);
            }
            break;
            case Yarn::Instruction_OpCode_ADD_OPTION:
            {
                // Adds an entry to the option list (see ShowOptions).
                // - opA = string: string ID for option to add
                // - opB = string: destination to go to if this option is selected
                // - opC = number: number of expressions on the stack to insert
                //   into the line
                // - opD = bool: whether the option has a condition on it (in which
                //   case a value should be popped off the stack and used to signal
                //   the game that the option should be not available)

                bool enabled = true;
                std::vector<Yarn::Operand> substitutions;

                int nOperands = instruction.operands_size();

                if (nOperands < 2)
                {
                    throw YarnException("Insufficient arguments to ADD_OPTION");
                }

                if (nOperands > 2)
                {
                    int substitutions = (int)get_float_operand(instruction, 2);

                    ///\todo -------------------------------------- NYI --------------------------------------

                }

                if ((nOperands > 3) && (get_bool_operand(instruction, 3)))
                {
                    Yarn::Operand& top = variableStack.top();

                    if ((!top.has_bool_value()) || (top.has_float_value() && (top.float_value() == 0.f)))
                    {
                        throw YarnException("ADD_OPTION: condition on top of stack not convertible to a bool value");
                    }

                    enabled = top.bool_value();
                    variableStack.pop();
                }

                Option opt =
                {
                    Line {get_string_operand(instruction,0), substitutions},
                    get_string_operand(instruction,1),
                    enabled
                };

                currentOptionsList.push_back(opt);
            }
            break;
            case Yarn::Instruction_OpCode_SHOW_OPTIONS:
            {
                assert(currentOptionsList.size());

                callbacks.onShowOptions(currentOptionsList);

                /*if ()
                {

                }
                else
                {
                    programState.runningState = ProgramState::AWAITING_INPUT;
                    callbacks.onShowOptions(currentOptionsList);
                }*/
            }
            break;
            case Yarn::Instruction_OpCode_PUSH_STRING:
            {
                if (!instruction.operands_size() >= 1)
                {
                    throw YarnException("Missing operand for PUSH_STRING instruction");
                }

                const Yarn::Operand& op = instruction.operands(0);

                if (!op.has_string_value())
                {
                    throw YarnException("Passed non-string operand to PUSH_STRING instruction");
                }

                variableStack.push(op);
            }
            break;
            case Yarn::Instruction_OpCode_PUSH_FLOAT:
            {
                if (!instruction.operands_size() >= 1)
                {
                    throw YarnException("Missing operand for PUSH_FLOAT instruction");
                }

                const Yarn::Operand& op = instruction.operands(0);

                if (!op.has_float_value())
                {
                    throw YarnException("Passed non-float operand to PUSH_FLOAT instruction");
                }

                variableStack.push(op);
            }
            break;
            case Yarn::Instruction_OpCode_PUSH_BOOL:
            {
                if (!instruction.operands_size() >= 1)
                {
                    throw YarnException("Missing operand for PUSH_BOOL instruction");
                }

                const Yarn::Operand& op = instruction.operands(0);

                if (!op.has_bool_value())
                {
                    throw YarnException("Passed non-bool operand to PUSH_BOOL instruction");
                }

                variableStack.push(op);
            }
            break;
            case Yarn::Instruction_OpCode_PUSH_NULL:
            {
                Yarn::Operand op;
                op.clear_value();

                variableStack.push(op);
            }
            break;
            case Yarn::Instruction_OpCode_JUMP_IF_FALSE:
            {
                // Jumps to the named position in the the node, if the top of the
                // stack is not null, zero or false.
                // opA = string: label name 

                const Yarn::Operand& top = variableStack.top();

                if (top.has_string_value() || (top.has_bool_value() && (top.bool_value() != false)) || (top.has_float_value() && (top.float_value() != 0.f)))
                {
                    const std::string& labelName = get_string_operand(instruction, 0);

                    std::int32_t translatedLabel = programState.currentNode->labels().at(labelName);

                    programState.setInstruction(translatedLabel);
                }
            }
            break;
            case Yarn::Instruction_OpCode_POP:
            {
                if (!variableStack.size())
                {
                    throw YarnException("POP instruction called with 0 stack size");
                }
                variableStack.pop();
            }
            break;
            case Yarn::Instruction_OpCode_CALL_FUNC:
            {
                // ------------------- TODO NYI WTF ------------------------ //

                int a = instruction.operands_size();

                const std::string& varname = get_string_operand(instruction, 0);

                auto operandCt = variableStack.top().float_value();

                variableStack.pop();

                auto rval = this->functions[varname](*this, operandCt);

                this->variableStack.push(rval);
            }
            break;
            case Yarn::Instruction_OpCode_PUSH_VARIABLE:
            {
                const std::string& varname = get_string_operand(instruction, 0);

                variableStack.push(variableStorage[varname]);
            }
            break;
            case Yarn::Instruction_OpCode_STORE_VARIABLE:
            {
                const std::string& varname = get_string_operand(instruction, 0);

                if (!variableStack.size())
                {
                    throw YarnException("STORE_VARIABLE instruction called with empty stack size");
                }

                variableStorage[varname] = variableStack.top();
            }
            break;
            case Yarn::Instruction_OpCode_STOP:
            {
                programState.runningState = ProgramState::STOPPED;

                callbacks.onProgramStopped();

                return;
            }
            break;
            case Yarn::Instruction_OpCode_RUN_NODE:
            {
                callbacks.onChangeNode();

                if (variableStack.size() && variableStack.top().has_string_value())
                {
                    const std::string& nodeName = variableStack.top().string_value();

                    loadNode(nodeName);

                    variableStack.pop();

                    return;
                }
                else
                {
                    throw YarnException("RUN_NODE instruction expected string variable on top of stack");
                }

            }
            break;
            default:
            {
                throw YarnException("Unknown instruction opcode");
                break;
            }
        };

         programState.advance();
    }

    struct ProgramState
    {
        const Yarn::Node* currentNode = nullptr;
        std::size_t instructionPointer = 0;

        enum RunningState { RUNNING, STOPPED, AWAITING_INPUT };

        RunningState runningState = RUNNING;

        const Yarn::Instruction& currentInstruction()
        {
            assert(currentNode);
            assert(currentNode->instructions_size() > (instructionPointer));

            return currentNode->instructions(instructionPointer);
        }

        void setInstruction(std::int32_t instruction)
        {
            /*
            if (!currentNode || (currentNode->instructions_size() > (instruction)))
            {
                throw YarnException("Invalid parameters for setInstruction()");
            }
            */

            instructionPointer = instruction;
        }

        const Yarn::Instruction& advance()
        {
            if (runningState != ProgramState::RUNNING)
            {
                throw YarnException("advance() called for next instruction when program is stopped");
            }

            if (currentNode)
            {
                if (currentNode->instructions_size() > (instructionPointer + 1))
                {
                    instructionPointer++;
                }
                else
                {
                    throw YarnException("nextInstruction() called when current node has no more instructions");
                }
            }
            else
            {
                throw YarnException("Current Node is nullptr in nextInstruction()");
            }

            return currentInstruction();
        }

        ProgramState()
        {
        }

        ProgramState(const Yarn::Node& node, std::size_t iptr = 0)
            :currentNode(&node),
            instructionPointer(iptr)
        {

        }
    };

    ProgramState programState;

    bool loadProgram(std::istream& is)
    {
        bool parsed = program.ParseFromIstream(&is);

        ///\todo set variable storage

        this->variableStorage = std::unordered_map<std::string, Yarn::Operand>(program.initial_values().begin(), program.initial_values().end());

        return parsed;
    }

    void populateFuncs()
    {
        functions["Number.Add"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_float_value(a + b);

            return rval;
        };

        functions["Number.Minus"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_float_value(a - b);

            return rval;
        };

        functions["Number.Multiply"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_float_value(a * b);

            return rval;
        };

        functions["Number.Divide"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_float_value(a / b);

            return rval;
        };

        functions["Number.Modulo"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_float_value(std::fmod(a, b));

            return rval;
        };

        functions["Number.LessThan"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_bool_value(a < b);

            return rval;
        };

        functions["Number.EqualTo"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            ///\todo float equality
            rval.set_bool_value(a == b);

            return rval;
        };

        functions["Number.GreaterThan"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            ///\todo float equality
            rval.set_bool_value(a > b);

            return rval;
        };

        functions["Number.GreaterThanOrEqualTo"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            ///\todo float equality
            rval.set_bool_value(a >= b);

            return rval;
        };

        functions["Number.LessThanOrEqualTo"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            ///\todo float equality
            rval.set_bool_value(a <= b);

            return rval;
        };

        functions["Bool.And"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().bool_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().bool_value();
            yarn.variableStack.pop();

            rval.set_bool_value(a && b);

            return rval;
        };

        functions["Bool.Or"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().bool_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().bool_value();
            yarn.variableStack.pop();

            rval.set_bool_value(a || b);

            return rval;
        };

        functions["Bool.Xor"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().bool_value();
            yarn.variableStack.pop();
            auto b = yarn.variableStack.top().bool_value();
            yarn.variableStack.pop();

            rval.set_bool_value(a && b);

            return rval;
        };

        functions["Bool.Not"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto a = yarn.variableStack.top().bool_value();
            yarn.variableStack.pop();
 
            rval.set_bool_value(!a);

            return rval;
        };
    }

    YarnMachine(std::istream& is)
    {
        populateFuncs();

        GOOGLE_PROTOBUF_VERIFY_VERSION;

        if (!loadProgram(is))
        {
            throw YarnException("Unable to load Yarn Program from stream");
        }
    }

    YarnMachine()
    {
        populateFuncs();

        GOOGLE_PROTOBUF_VERIFY_VERSION;
    }

    bool loadNode(const std::string& node)
    {
        const Yarn::Node& nodeRef = program.nodes().at(node);
        programState = ProgramState(nodeRef);
        return true; ///\todo true
    }
};
