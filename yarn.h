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

    /// settings passed into constructor of YarnVM
    struct Settings
    {
        std::mt19937::result_type randomSeed = std::mt19937::default_seed;
        bool enableExceptions = true;
    };

    /// information on line to run
    struct Line
    {
        std::string id; ///< unique identifier for the line - actual text and metadata are stored in lines database object & retrieved from there
        std::vector<Yarn::Operand> substitutions;
    };

    /// Information on an option presented to the user
    struct Option
    {
        Line line;               ///< line to display to the user for this option
        std::string destination; ///< destination to go to if this option is selected
        bool  enabled;           ///< whether the option has a condition on it(in which case a value should be popped off the stack and used to signal the game that the option should be not available)
    };

    /// Variable stack used by the VM
    class Stack : public std::stack <Yarn::Operand>
    {
    public:

#ifdef YARN_SERIALIZATION_JSON
        nlohmann::json to_json() const;
        void from_json(const nlohmann::json& js);
#endif
    };


    typedef std::function<void(void)> YarnCallback;
    typedef std::function<Yarn::Operand(YarnVM& yarn, int parameterCount)> YarnFunction;
    typedef std::vector<Option> OptionsList;

    /// Current State of the VM
    enum RunningState { RUNNING, STOPPED, AWAITING_INPUT, ASLEEP};

    #define YARN_CALLBACK(name) YarnCallback name = NIL_CALLBACK;
    #define YARN_FUNC(x) functions[ x ] = [](YarnVM& yarn, int parameters)->Yarn::Operand

    /// User bindable callbacks available to the VM.
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

    Stack variableStack;

    std::unordered_map<std::string, Yarn::Operand> variableStorage;

    // https://stackoverflow.com/questions/27727012/c-stdmt19937-and-rng-state-save-load-portability
    // we want to be able to serialize / deserialize the rng state and continue deterministically
    // this means we can't use std::default_random_engine for the generator since this is implementation defined.
    std::mt19937 generator;

    std::size_t instructionPointer = 0;

    RunningState runningState = RUNNING;

    Settings settings;

    long long time = 0;
    long long waitUntilTime = 0;

    // --- the following members are not directly serializable but store some derived properties

    Yarn::Program program;
    const Yarn::Node* currentNode = nullptr;

    // --- The following members are NOT part of the serializable state! ---
    // c++ callbacks and function tables are to be populated directly by the client / game code

    std::unordered_map<std::string, YarnFunction> functions;

    YarnCallbacks callbacks;

    // --- Public method interface below.  Called by your Dialogue Runner class which owns this VM ---

    YarnVM(const Settings& setts = {});

#ifdef YARN_SERIALIZATION_JSON

    void fromJS(const nlohmann::json& js);

    nlohmann::json toJS() const;

#endif

    bool loadNode(const std::string& node);

    bool loadProgram(std::istream& is);

    void setTime(long long timeIn);

    void incrementTime(long long dt) { setTime(time + dt); }

    void waitUntil(long long t) { waitUntilTime = t; runningState = RunningState::ASLEEP; }

    void setWaitTime(long long t) { waitUntil(time + t); }

    void selectOption(const Option& option);

    void selectOption(int selection);

    void processInstruction(const Yarn::Instruction& instruction);

    const Yarn::Instruction& currentInstruction();

    void setInstruction(std::int32_t instruction);

    const Yarn::Instruction& advance(); ///< advance the instruction pointer and return the next instruction

    unsigned int visitedCount(const std::string& node);

    protected:

    void populateFuncs();

    std::string get_string_operand(const Yarn::Instruction& instruction, int index);

    bool get_bool_operand(const Yarn::Instruction& instruction, int index);

    float get_float_operand(const Yarn::Instruction& instruction, int index);
};
}
