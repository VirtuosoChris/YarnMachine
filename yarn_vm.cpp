#include <yarn_spinner.pb.h>
#include <yarn_vm.h>

#ifdef YARN_SERIALIZATION_JSON
#include <fstream>
#include <json.hpp>

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
                float tmp = j["value"].get<float>();
                opt.set_float_value(tmp);
            }
            else if (typestr == "string")
            {
                opt.set_string_value(j["value"].get<std::string>());
            }
            else if (typestr == "bool")
            {
                opt.set_bool_value(j["value"].get<bool>());
            }
            else
            {
            }
        }
    };
}

namespace Yarn
{
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Yarn::YarnVM::Settings, randomSeed, enableExceptions);
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Yarn::YarnVM::Line, id, substitutions);
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Yarn::YarnVM::Option, line, destination, enabled);
}


namespace Yarn
{
    inline void to_json(nlohmann::json& j, const Yarn::YarnVM::Stack& p)
    {
        j = p.to_json();
    }

    inline void from_json(const nlohmann::json& j, Yarn::YarnVM::Stack& p)
    {
        p.from_json(j);
    }
}

#endif


/// write all variables, types, and values to an ostream as key:value pairs, one variable per line
std::ostream& operator<<(std::ostream& str, const Yarn::Operand& op)
{
    if (op.has_bool_value())
    {
        str << op.bool_value();
    }
    else if (op.has_float_value())
    {
        str << op.float_value();
    }
    else if (op.has_string_value())
    {
        str << op.string_value();
    }
    else
    {
        // this should never happen
        str << "UNDEFINED";
    }

    return str;
}

struct StaticContext
{
    StaticContext() {}
    ~StaticContext() { google::protobuf::ShutdownProtobufLibrary(); }
};

using namespace Yarn;

unsigned int YarnVM::visitedCount(const std::string& node)
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

void YarnVM::selectOption(const Option& option)
{
    runningState = RUNNING;

    Yarn::Operand op;
    op.set_string_value(option.destination);
    variableStack.push(op);

    currentOptionsList.clear();

    assert(currentNode);

#if 0
    std::int32_t translatedLabel = currentNode->labels().at(option.destination);

    currentOptionsList.resize(0);

    setInstruction(translatedLabel);
#endif
}

void YarnVM::selectOption(int selection)
{
    if (selection >= currentOptionsList.size())
    {
        YARN_EXCEPTION("Invalid option selected");
    }

    selectOption(currentOptionsList[selection]);
}

std::string YarnVM::get_string_operand(const Yarn::Instruction& instruction, int index)
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

bool YarnVM::get_bool_operand(const Yarn::Instruction& instruction, int index)
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

float YarnVM::get_float_operand(const Yarn::Instruction& instruction, int index)
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

const Yarn::Instruction& YarnVM::advance()
{
    if (runningState == STOPPED)
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

void YarnVM::populateFuncs()
{
    functions["Number.Add"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto a = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();
        auto b = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();

        rval.set_float_value(a + b);

        return rval;
    };

    functions["Number.Minus"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto b = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();
        auto a = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();

        rval.set_float_value(a - b);

        return rval;
    };

    functions["Number.Multiply"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto a = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();
        auto b = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();

        rval.set_float_value(a * b);

        return rval;
    };

    functions["Number.Divide"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto b = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();
        auto a = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();

        rval.set_float_value(a / b);

        return rval;
    };

    functions["Number.Modulo"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto b = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();
        auto a = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();

        rval.set_float_value(std::fmod(a, b));

        return rval;
    };

    functions["Number.LessThan"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto b = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();
        auto a = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();

        rval.set_bool_value(a < b);

        return rval;
    };

    functions["Number.EqualTo"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto a = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();
        auto b = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();

        rval.set_bool_value(a == b);

        return rval;
    };

    functions["Number.GreaterThan"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto b = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();
        auto a = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();

        rval.set_bool_value(a > b);

        return rval;
    };

    functions["Number.GreaterThanOrEqualTo"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto b = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();
        auto a = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();

        rval.set_bool_value(a >= b);

        return rval;
    };

    functions["Number.LessThanOrEqualTo"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto b = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();
        auto a = yarn.variableStack.top().float_value();
        yarn.variableStack.pop();

        rval.set_bool_value(a <= b);

        return rval;
    };

    functions["Bool.And"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto a = yarn.variableStack.top().bool_value();
        yarn.variableStack.pop();
        auto b = yarn.variableStack.top().bool_value();
        yarn.variableStack.pop();

        rval.set_bool_value(a && b);

        return rval;
    };

    functions["Bool.Or"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto a = yarn.variableStack.top().bool_value();
        yarn.variableStack.pop();
        auto b = yarn.variableStack.top().bool_value();
        yarn.variableStack.pop();

        rval.set_bool_value(a || b);

        return rval;
    };

    functions["Bool.Xor"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
    {
        Yarn::Operand rval;

        auto b = yarn.variableStack.top().bool_value();
        yarn.variableStack.pop();
        auto a = yarn.variableStack.top().bool_value();
        yarn.variableStack.pop();

        rval.set_bool_value(a && b);

        return rval;
    };

    functions["Bool.Not"] = [](YarnVM& yarn, int parameters)->Yarn::Operand
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

void YarnVM::setTime(long long timeIn)
{
    time = timeIn;

    // check if sleep time is expired
    if (runningState == ASLEEP && (time >= waitUntilTime))
    {
        runningState = RUNNING;
    }
}

const Yarn::Instruction& YarnVM::currentInstruction()
{
    assert(currentNode);
    auto instructionCtNode = currentNode->instructions_size();
    assert(instructionCtNode > (instructionPointer));

    return currentNode->instructions(instructionPointer);
}

void YarnVM::setInstruction(std::int32_t instruction)
{
    if (!currentNode)
    {
        YARN_EXCEPTION("current node is null in setInstruction()");
    }

    if (instruction >= currentNode->instructions_size())
    {
        YARN_EXCEPTION("Invalid instruction pointer parameter for setInstruction()");
    }

    instructionPointer = instruction;
}


YarnVM::YarnVM(const YarnVM::Settings& setts)
    :
    settings(setts),
    generator(setts.randomSeed)
{
    static StaticContext sc;

    populateFuncs();

    GOOGLE_PROTOBUF_VERIFY_VERSION;
}

bool YarnVM::loadNode(const std::string& node)
{
    // node not found check
    if (program.nodes().find(node) == program.nodes().end())
    {
        YARN_EXCEPTION("loadNode() failure : node not found : " + node);
        return false;
    }

    const Yarn::Node& nodeRef = program.nodes().at(node);

    const Yarn::Node* prevNode = currentNode;
    currentNode = &nodeRef;
    instructionPointer = 0;

    runningState = RUNNING;

    if (callbacks) callbacks->onChangeNode(prevNode, currentNode);

    return true;
}


bool YarnVM::loadProgram(const std::string& yarncFileIn)
{
    yarncFile = yarncFileIn;

    std::ifstream is(yarncFile, std::ios::binary | std::ios::in);

    assert(is.is_open());

    bool parsed = program.ParseFromIstream(&is);

    this->variableStorage = std::unordered_map<std::string, Yarn::Operand>(program.initial_values().begin(), program.initial_values().end());

    return parsed;
}

#ifdef YARN_SERIALIZATION_JSON

void YarnVM::fromJS(const nlohmann::json& js)
{
    settings = js["settings"].get<YarnVM::Settings>();

    const std::string& generatorStr = js["generator"].get<std::string>();
    std::istringstream sstr(generatorStr);
    sstr >> generator;

    time = js["time"].get<long long>();
    waitUntilTime = js["waitUntilTime"].get<long long>();

    this->loadProgram(js["yarncFile"].get<std::string>()); // loading the program sets the initial variables
    this->loadNode(js["currentNode"]);

    variableStorage = js["variables"].get< std::unordered_map<std::string, Yarn::Operand>>();
    variableStack = js["stack"].get<YarnVM::Stack>();
    currentOptionsList = js["options"].get<OptionsList>();

    instructionPointer = js["instructionPointer"].get<std::size_t>();
    runningState = (RunningState)js["runningState"].get<int>();

    if (this->runningState == AWAITING_INPUT)
    {
        if (callbacks) callbacks->onPresentOptions(this->currentOptionsList);
    }
}

nlohmann::json YarnVM::toJS() const
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

        rval["yarncFile"] = this->yarncFile;
    }

    { // serialize the time
        rval["time"] = time;
        rval["waitUntilTime"] = waitUntilTime;
    }

    return rval;
}

nlohmann::json Yarn::YarnVM::Stack::to_json() const
{
    return nlohmann::json(this->c);
}

void Yarn::YarnVM::Stack::from_json(const nlohmann::json& js)
{
    this->c = js.get<YarnVM::Stack::container_type>();
}
#endif

