#pragma once

#define YARN_SERIALIZATION_JSON

#include <functional>
#include <stack>
#include <iostream>
#include <random>

#include <yarn_spinner.pb.h>


#define YarnException std::runtime_error

//#define CALLBACK_ARGS_PROTOTYPE void
//#define CALLBACK_ARGS NULL
#define NIL_CALLBACK [](){}

#define YARN_EXCEPTION(x) if (settings.enableExceptions) { throw YarnException( x ); }

#ifdef YARN_SERIALIZATION_JSON
namespace nlohmann
{
    template<>
    struct adl_serializer<Yarn::Operand>
    {
        static void to_json(json& js, const Yarn::Operand& op)
        {
            if (op.has_bool_value())
            {
                js = { {"value", op.bool_value()}, {"type", "bool"} };
            }

            else if (op.has_float_value())
            {
                js = { {"value", op.float_value()}, {"type", "float"} };
            }

            else if (op.has_float_value())
            {
                js = { {"value", op.string_value()}, {"type", "string"} };
            }

            else js = { {"value", "UNDEFINED"}, {"type", "UNDEFINED"} };
 
        }

        static void from_json(const json& j, Yarn::Operand& opt)
        {
            const std::string& typestr = j["type"].get<std::string>();

            if (typestr == "float")
            {
                opt.set_float_value ( j.get<float>());
            }
            else if (typestr == "string")
            {
                opt.set_string_value(j.get<std::string>());
            }
            else if (typestr == "bool")
            {
                opt.set_bool_value(j.get<bool>());
            }
            else
            {
                ///\todo except
            }
        }
    };
}
#endif

struct YarnVMSettings
{
    std::mt19937::result_type randomSeed = std::mt19937::default_seed;
    bool enableExceptions = true;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(YarnVMSettings, randomSeed, enableExceptions);
};

class YarnStack : public std::stack <Yarn::Operand>
{
public:

#ifdef YARN_SERIALIZATION_JSON
    nlohmann::json to_json() const
    {
        return nlohmann::json(this->_Get_container());
    }

    void from_json(const nlohmann::json& js)
    {
        this->c = js.get<YarnStack::container_type>();
    }
#endif
};

#ifdef YARN_SERIALIZATION_JSON
void to_json(nlohmann::json& j, const YarnStack& p)
{
    j = p.to_json();
}

void from_json(const nlohmann::json& j, YarnStack& p)
{
    p.from_json(j);
}
#endif

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

#ifdef YARN_SERIALIZATION_JSON
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Line, id, substitutions);
#endif
    };

    struct Option
    {
        Line line;
        std::string destination; // destination to go to if this option is selected
        bool  enabled;           // whether the option has a condition on it(in which case a value should be popped off the stack and used to signal the game that the option should be not available) (????????????)

#ifdef YARN_SERIALIZATION_JSON
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Option, line, destination, enabled);
#endif
    };

    enum RunningState { RUNNING, STOPPED, AWAITING_INPUT };

    typedef std::function<void(void)> YarnCallback;

    typedef std::function<Yarn::Operand(YarnMachine& yarn, int parameterCount)> YarnFunction; ///\todo arguments
    typedef std::vector<Option> OptionsList;

    #define YARN_CALLBACK(name) YarnCallback name = NIL_CALLBACK;
    #define YARN_FUNC(x) functions[ x ] = [](YarnMachine& yarn, int parameters)->Yarn::Operand

    struct YarnCallbacks
    {
        YARN_CALLBACK(onProgramStopped);
        YARN_CALLBACK(onChangeNode);

        std::function<void(const YarnMachine::Line&)> onLine = [](const YarnMachine::Line& line) {};
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

    // --- the following members are not directly serializable but store some derived properties

    Yarn::Program program;
    const Yarn::Node* currentNode = nullptr;

    // --- The following members are NOT part of the serializable state! ---

    std::unordered_map<std::string, YarnFunction> functions;

    YarnCallbacks callbacks;

#ifdef YARN_SERIALIZATION_JSON

    void fromJS(const nlohmann::json& js)
    {
        settings = js["settings"].get<YarnVMSettings>();

        const std::string& generatorStr = js["generator"].get<std::string>();
        std::istringstream sstr(generatorStr);
        sstr >> generator;

        variableStorage = js["variables"].get< std::unordered_map<std::string, Yarn::Operand>>();
        variableStack = js["stack"].get<YarnStack>();
        currentOptionsList = js["options"].get<OptionsList>();


        instructionPointer = js["instructionPointer"].get<std::size_t>();
        runningState = (RunningState)js["runningState"].get<int>();

        ///\todo -- load the node from the node name, set the callback.

        ///\todo load the program and database from input URI
    }

    nlohmann::json toJS() const
    {
        nlohmann::json rval;

        { // serialize the settings

            rval["settings"] = nlohmann::json(settings);
        }

        { // serialize the rng
            std::stringstream sstr;
            sstr << generator;
            rval["generator"] = sstr.str();
        }

        { // serialize variable storage
            rval["variables"] = nlohmann::json(variableStorage);
        }

        { // serialize the stack
            // std::list <Yarn::Operand> variableStack2;
            rval["stack"] = nlohmann::json(variableStack);
        }

        { // serialize the current options list

            rval["options"] = nlohmann::json(currentOptionsList);
        }

        { // serialize the program state 
            assert(currentNode != nullptr && currentNode->name().length());

            if (currentNode != nullptr) { rval["currentNode"] = currentNode->name(); }

            rval["instructionPointer"] = instructionPointer;

            rval["runningState"] = (int)runningState;
        }

        return rval;
    }
#endif

    /// write all variables, types, and values to an ostream as key:value pairs, one variable per line
    std::ostream& logVariables(std::ostream& str)
    {
        for (auto it = variableStorage.begin(); it != variableStorage.end(); it++)
        {

            str << "name:" << it->first << "\ttype:";

            if (it->second.has_bool_value())
            {
                str << "bool\tvalue:" << it->second.bool_value() << '\n';
            }
            else if (it->second.has_float_value())
            {
                str << "float\tvalue:" << it->second.float_value() << '\n';
            }
            else if (it->second.has_string_value())
            {
                str << "string\tvalue:" << it->second.string_value() << '\n';
            }
            else
            {
                // this should never happen
                str << "UNDEFINED\tvalue:UNDEFINED\n";
            }
        }

        return str;
    }

    void selectOption(const Option& option)
    {
        runningState = RUNNING;

        std::int32_t translatedLabel = currentNode->labels().at(option.destination);

        currentOptionsList.resize(0);

        setInstruction(translatedLabel);
    }

    void selectOption(int selection)
    {
        if (selection >= currentOptionsList.size())
        {
            YARN_EXCEPTION("Invalid option selected");
        }

        selectOption(currentOptionsList[selection]);
    }


    std::string get_string_operand(const Yarn::Instruction& instruction, int index)
    {
        if (instruction.operands_size() <= index)
        {
            YARN_EXCEPTION("get_string_operand() : Operand out of bounds");
        }

        const Yarn::Operand& op = instruction.operands(index);

        if (!op.has_string_value())
        {
            YARN_EXCEPTION("get_string_operand() : operand doesn't have a string value");
        }

        return op.string_value();
    }

    bool get_bool_operand(const Yarn::Instruction& instruction, int index)
    {
        if (instruction.operands_size() <= index)
        {
            YARN_EXCEPTION("get_bool_operand() : Operand out of bounds");
        }

        const Yarn::Operand& op = instruction.operands(index);

        if (!op.has_bool_value())
        {
            YARN_EXCEPTION("get_bool_operand() : operand doesn't have a bool value");
        }

        return op.bool_value();
    }

    float get_float_operand(const Yarn::Instruction& instruction, int index)
    {
        if (instruction.operands_size() <= index)
        {
            YARN_EXCEPTION("get_float_operand() : Operand out of bounds");
        }

        const Yarn::Operand& op = instruction.operands(index);

        if (!op.has_float_value())
        {
            YARN_EXCEPTION("get_float_operand() : operand doesn't have a float value");
        }

        return op.float_value();
    }


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

    const Yarn::Instruction& advance()
    {
        if (runningState != RUNNING)
        {
            YARN_EXCEPTION("advance() called for next instruction when program is stopped");
        }

        if (currentNode)
        {
            if (currentNode->instructions_size() > (instructionPointer + 1))
            {
                instructionPointer++;
            }
            else
            {
                YARN_EXCEPTION("nextInstruction() called when current node has no more instructions");
            }
        }
        else
        {
            YARN_EXCEPTION("Current Node is nullptr in nextInstruction()");
        }

        return currentInstruction();
    }

    bool loadProgram(std::istream& is)
    {
        bool parsed = program.ParseFromIstream(&is);

        ///\todo set variable storage

        this->variableStorage = std::unordered_map<std::string, Yarn::Operand>(program.initial_values().begin(), program.initial_values().end());

        return parsed;
    }

    unsigned int visitedCount(const std::string& node)
    {
        const std::string& nodeTrackerVariable = "$Yarn.Internal.Visiting." + node;

        auto it = this->variableStorage.find(nodeTrackerVariable);

        if (it == variableStorage.end())
        {
            return 0;
        }

        assert(it->second.has_float_value());

        return it->second.float_value();
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

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto a = yarn.variableStack.top().float_value();
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

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_float_value(a / b);

            return rval;
        };

        functions["Number.Modulo"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_float_value(std::fmod(a, b));

            return rval;
        };

        functions["Number.LessThan"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto a = yarn.variableStack.top().float_value();
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

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_bool_value(a > b);

            return rval;
        };

        functions["Number.GreaterThanOrEqualTo"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_bool_value(a >= b);

            return rval;
        };

        functions["Number.LessThanOrEqualTo"] = [](YarnMachine& yarn, int parameters)->Yarn::Operand
        {
            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();
            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

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

            auto b = yarn.variableStack.top().bool_value();
            yarn.variableStack.pop();
            auto a = yarn.variableStack.top().bool_value();
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

        /*
        visited(string node_name)
        visited returns a boolean value of true if the node with the title of node_name has been entered and exited at least once before, otherwise returns false.
        Will return false if node_name doesn't match a node in project.
        */
        YARN_FUNC("visited")
        {
            Yarn::Operand rval;

            assert(parameters == 1);

            auto node = yarn.variableStack.top().string_value();
            yarn.variableStack.pop();

            unsigned int visitedCount = yarn.visitedCount(node);

            rval.set_bool_value(visitedCount > 0);

            return rval;
        };

        /*
        visited_count(string node_name)
        visted_count returns a number value of the number of times the node with the title of node_name has been entered and exited, otherwise returns 0.
        Will return 0 if node_name doesn't match a node in project.
        */
        YARN_FUNC("visited_count")
        {
            assert(parameters == 1);

            Yarn::Operand rval;

            auto node = yarn.variableStack.top().string_value();
            yarn.variableStack.pop();

            unsigned int visitedCount = yarn.visitedCount(node);

            rval.set_float_value(visitedCount);

            return rval;
        };

        /*
        random()
        random returns a random number between 0 and 1 each time you call it.
        */
        YARN_FUNC("random")
        {
            assert(parameters == 0);

            Yarn::Operand rval;

            std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

            rval.set_float_value(distribution(yarn.generator));

            return rval;
        };

        /*
        random_range(number a, number b)
        random_range returns a random integer between a and b, inclusive.
        */
        YARN_FUNC("random_range")
        {
            assert(parameters == 2);

            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            auto a = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            std::uniform_int_distribution<> distribution(a, b);

            assert(a <= b);

            rval.set_float_value(distribution(yarn.generator));

            return rval;
        };

        /*
        dice(number sides)
        dice returns a random integer between 1 and sides, inclusive.
        For example, dice(6) returns a number between 1 and 6, just like rolling a six-sided die.
        */
        YARN_FUNC("dice")
        {
            assert(parameters == 1);

            Yarn::Operand rval;

            auto sides = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            std::uniform_int_distribution<> distribution(1, sides);

            assert(sides > 0);

            rval.set_float_value(distribution(yarn.generator));

            return rval;
        };

        /*
        round(number n)
        round rounds n to the nearest integer.
        */
        YARN_FUNC("round")
        {
            assert(parameters == 1);

            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_float_value(std::round(b));

            return rval;
        };

        /*
        round_places(number n, number places)
        round_places rounds n to the nearest number with places decimal points.
        */
        YARN_FUNC("round_places")
        {
            assert(parameters == 2);

            Yarn::Operand rval;

            auto places = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            auto n = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            int scale = 1;
            for (int i = 0; i < places; i++)
            {
                scale *= 10;
            }

            auto downscale = 1. / scale;

            auto tmp = std::round(n * scale);

            auto tmp2 = tmp * downscale;

            rval.set_float_value(tmp2);

            return rval;
        };

        /*
        floor(number n)
        floor rounds n down to the nearest integer, towards negative infinity.
        */
        YARN_FUNC("floor")
        {
            assert(parameters == 1);

            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_float_value(std::floor(b));

            return rval;
        };

        /*
        ceil(number n)
        ceil rounds n up to the nearest integer, towards positive infinity
        */
        YARN_FUNC("ceil")
        {
            assert(parameters == 1);

            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_float_value(std::ceil(b));

            return rval;
        };

        /*
        inc(number n)
        inc rounds n up to the nearest integer. If n is already an integer, inc returns n+1.
        */
        YARN_FUNC("inc")
        {
            assert(parameters == 1);

            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            auto ceilb = std::ceil(b);

            rval.set_float_value(ceilb == b ? ceilb + 1. : ceilb);

            return rval;
        };

        /*
        dec(number n)
        inc rounds n down to the nearest integer. If n is already an integer, inc returns n-1.
        */
        YARN_FUNC("dec")
        {
            assert(parameters == 1);

            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            auto ceilb = std::floor(b);

            rval.set_float_value(ceilb == b ? ceilb - 1. : ceilb);

            return rval;
        };

        /*
        decimal(number n)
        decimal returns the decimal portion of n. This will always be a number between 0 and 1. For example, decimal(4.51) will return 0.51.
        */
        YARN_FUNC("decimal")
        {
            assert(parameters == 1);

            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            float tmp = 0.f;
            rval.set_float_value(std::modf(b, &tmp));

            return rval;
        };

        /*
        int(number n)
        int rounds n down to the nearest integer, towards zero
        */
        YARN_FUNC("int")
        {
            assert(parameters == 1);

            Yarn::Operand rval;

            auto b = yarn.variableStack.top().float_value();
            yarn.variableStack.pop();

            rval.set_float_value(std::trunc(b));

            return rval;
        };

    }

    YarnMachine(std::istream& is, const YarnVMSettings& setts = {})
        :
        settings(setts),
        generator(setts.randomSeed)
    {
        populateFuncs();

        GOOGLE_PROTOBUF_VERIFY_VERSION;

        if (!loadProgram(is))
        {
            YARN_EXCEPTION("Unable to load Yarn Program from stream");
        }
    }

    YarnMachine(const YarnVMSettings& setts = {})
        :
        settings(setts),
        generator(setts.randomSeed)
    {
        populateFuncs();

        GOOGLE_PROTOBUF_VERIFY_VERSION;
    }

    bool loadNode(const std::string& node)
    {
        const Yarn::Node& nodeRef = program.nodes().at(node);

        //programState = ProgramState(nodeRef);

        currentNode = &nodeRef;

        callbacks.onChangeNode();

        return true; ///\todo true
    }
};
