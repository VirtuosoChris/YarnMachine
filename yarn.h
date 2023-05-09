#pragma once

#include <functional>
#include <stack>
#include <iostream>
#include <random>

#include <yarn_spinner.pb.h>

#ifdef YARN_SERIALIZATION_JSON
#include <json.hpp>
#endif

#define YarnException std::runtime_error

#define NIL_CALLBACK [](){}

#define YARN_EXCEPTION(x) if (settings.enableExceptions) { throw YarnException( x ); }

namespace Yarn
{

struct YarnVMSettings
{
    std::mt19937::result_type randomSeed = std::mt19937::default_seed;
    bool enableExceptions = true;
};

class YarnStack : public std::stack <Yarn::Operand>
{
public:

#ifdef YARN_SERIALIZATION_JSON
    nlohmann::json to_json() const;

    void from_json(const nlohmann::json& js);
#endif
};



struct YarnVM
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

    enum RunningState { RUNNING, STOPPED, AWAITING_INPUT, ASLEEP};

    typedef std::function<void(void)> YarnCallback;

    typedef std::function<Yarn::Operand(YarnVM& yarn, int parameterCount)> YarnFunction; ///\todo arguments
    typedef std::vector<Option> OptionsList;

    #define YARN_CALLBACK(name) YarnCallback name = NIL_CALLBACK;
    #define YARN_FUNC(x) functions[ x ] = [](YarnVM& yarn, int parameters)->Yarn::Operand

    struct YarnCallbacks
    {
        YARN_CALLBACK(onProgramStopped);
        YARN_CALLBACK(onChangeNode);

        std::function<void(const YarnVM::Line&)> onLine = [](const YarnVM::Line& line) {};
        std::function<void(const std::string&)> onCommand = [](const std::string&) {};
        std::function<void(const OptionsList&)> onShowOptions = [](const OptionsList&) {};
    };

    /// --- VM state members which are serializable! ---

    OptionsList currentOptionsList;

    YarnStack variableStack;

    std::unordered_map<std::string, Yarn::Operand> variableStorage;

    // https://stackoverflow.com/questions/27727012/c-stdmt19937-and-rng-state-save-load-portability
    // we want to be able to serialize / deserialize the rng state and continue deterministically
    // this means we can't use std::default_random_engine for the generator since this is implementation defined.
    std::mt19937 generator;

    std::size_t instructionPointer = 0;

    RunningState runningState = RUNNING;

    YarnVMSettings settings;

    long long time;
    long long waitUntilTime;

    // --- the following members are not directly serializable but store some derived properties

    Yarn::Program program;
    const Yarn::Node* currentNode = nullptr;

    // --- The following members are NOT part of the serializable state! ---
    // c++ callbacks and function tables are to be populated directly by the client / game code

    std::unordered_map<std::string, YarnFunction> functions;

    YarnCallbacks callbacks;

    void setTime(long long timeIn)
    {
        time = timeIn;

        // check if sleep time is expired
        if (runningState == ASLEEP && (time >= waitUntilTime)) { runningState = RUNNING; }
    }

    void incrementTime(long long dt) { setTime(time + dt); }

    void waitUntil(long long t) { waitUntilTime = t; runningState = RunningState::ASLEEP; }

    void setWaitTime(long long t) { waitUntil(time + t); }

#ifdef YARN_SERIALIZATION_JSON

    void fromJS(const nlohmann::json& js);

    nlohmann::json toJS() const;

#endif

    void selectOption(const Option& option);

    void selectOption(int selection);

    std::string get_string_operand(const Yarn::Instruction& instruction, int index);

    bool get_bool_operand(const Yarn::Instruction& instruction, int index);

    float get_float_operand(const Yarn::Instruction& instruction, int index);


    void processInstruction(const Yarn::Instruction& instruction)
    {
        if (!currentNode)
        {
            YARN_EXCEPTION("Current node is nullptr in processInstruction()");
        }

        //bool jumpInstruction = false;
        auto op = instruction.opcode();

        switch (op)
        {
            case Yarn::Instruction_OpCode_JUMP_TO:
            {
                const std::string& jumpLabel = get_string_operand(instruction, 0);

                // -- hope this syntax is right for this lib!
                if (currentNode->labels().find(jumpLabel) == currentNode->labels().end())
                {
                    YARN_EXCEPTION("JUMP_TO : Jump label doesn't exist in current node");
                }

                std::int32_t translatedLabel = currentNode->labels().at(jumpLabel);

                setInstruction(translatedLabel);

                return;
            }
            break;
            case Yarn::Instruction_OpCode_JUMP:
            {
                const Yarn::Operand& stackTop = variableStack.top();

                if ((variableStack.size() == 0) || !stackTop.has_string_value())
                {
                    YARN_EXCEPTION("Stack top doesn't have a string value in JUMP instruction");
                }

                const std::string& jumpLoc = stackTop.string_value();

                std::int32_t translatedLabel = currentNode->labels().at(jumpLoc);

                setInstruction(translatedLabel);

                return;
            }
            break;
            case Yarn::Instruction_OpCode_RUN_LINE:
            {
                //std::cout << "run_line with operand ct : " << instruction.operands_size() << std::endl;
                const std::string& lineID = get_string_operand(instruction, 0);
                //std::cout << get_string_operand(instruction, 0) << std::endl;
                //std::cout << get_float_operand(instruction, 1) << std::endl;

                int substitutions = get_float_operand(instruction, 1);

                YarnVM::Line l;
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
                    YARN_EXCEPTION("Insufficient arguments to ADD_OPTION");
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
                        YARN_EXCEPTION("ADD_OPTION: condition on top of stack not convertible to a bool value");
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

                ///\todo
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
                    YARN_EXCEPTION("Missing operand for PUSH_STRING instruction");
                }

                const Yarn::Operand& op = instruction.operands(0);

                if (!op.has_string_value())
                {
                    YARN_EXCEPTION("Passed non-string operand to PUSH_STRING instruction");
                }

                variableStack.push(op);
            }
            break;
            case Yarn::Instruction_OpCode_PUSH_FLOAT:
            {
                if (!instruction.operands_size() >= 1)
                {
                    YARN_EXCEPTION("Missing operand for PUSH_FLOAT instruction");
                }

                const Yarn::Operand& op = instruction.operands(0);

                if (!op.has_float_value())
                {
                    YARN_EXCEPTION("Passed non-float operand to PUSH_FLOAT instruction");
                }

                variableStack.push(op);
            }
            break;
            case Yarn::Instruction_OpCode_PUSH_BOOL:
            {
                if (!instruction.operands_size() >= 1)
                {
                    YARN_EXCEPTION("Missing operand for PUSH_BOOL instruction");
                }

                const Yarn::Operand& op = instruction.operands(0);

                if (!op.has_bool_value())
                {
                    YARN_EXCEPTION("Passed non-bool operand to PUSH_BOOL instruction");
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
                }
                else
                {
                    const std::string& labelName = get_string_operand(instruction, 0);

                    std::int32_t translatedLabel = currentNode->labels().at(labelName);

                    setInstruction(translatedLabel);
                }
            }
            break;
            case Yarn::Instruction_OpCode_POP:
            {
                if (!variableStack.size())
                {
                    YARN_EXCEPTION("POP instruction called with 0 stack size");
                }
                variableStack.pop();
            }
            break;
            case Yarn::Instruction_OpCode_CALL_FUNC:
            {
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
                    YARN_EXCEPTION("STORE_VARIABLE instruction called with empty stack size");
                }

                variableStorage[varname] = variableStack.top();
            }
            break;
            case Yarn::Instruction_OpCode_STOP:
            {
                runningState = STOPPED;

                callbacks.onProgramStopped();

                return;
            }
            break;
            case Yarn::Instruction_OpCode_RUN_NODE:
            {
                if (variableStack.size() && variableStack.top().has_string_value())
                {
                    const std::string& nodeName = variableStack.top().string_value();

                    loadNode(nodeName);

                    variableStack.pop();

                    return;
                }
                else
                {
                    YARN_EXCEPTION("RUN_NODE instruction expected string variable on top of stack");
                }

            }
            break;
            default:
            {
                YARN_EXCEPTION("Unknown instruction opcode");
                break;
            }
        };

        advance();
    }

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

    const Yarn::Instruction& advance(); ///< advance the instruction pointer and return the next instruction

    bool loadProgram(std::istream& is)
    {
        bool parsed = program.ParseFromIstream(&is);

        ///\todo set variable storage

        this->variableStorage = std::unordered_map<std::string, Yarn::Operand>(program.initial_values().begin(), program.initial_values().end());

        return parsed;
    }

    unsigned int visitedCount(const std::string& node);

    void populateFuncs();

    YarnVM(const YarnVMSettings& setts = {})
        :
        settings(setts),
        generator(setts.randomSeed)
    {
        populateFuncs();

        GOOGLE_PROTOBUF_VERIFY_VERSION;
    }

    bool loadNode(const std::string& node)
    {
        // node not found check
        if (program.nodes().find(node) == program.nodes().end())
        {
            YARN_EXCEPTION("loadNode() failure : node not found : " + node);
            return false;
        }

        const Yarn::Node& nodeRef = program.nodes().at(node);

        currentNode = &nodeRef;

        callbacks.onChangeNode();

        return true;
    }
};
}
