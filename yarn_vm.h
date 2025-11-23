#pragma once

/**
 * @file yarn_vm.h
 *
 * @brief Yarn Spinner C++ Class for running compiled .yarnc files on a virtual
 * machine
 *
 * @author Christopher Pugh
 * Contact: chris@virtuosoengine.com
 *
 * A yarn virtual machine intended to be used with a Yarn Dialogue runner class.
 * This runs a .yarnc file of path provided to the loadProgram() method
 * it has callbacks for different events, such as running a line, running a
 * command, changing nodes, showing options, etc. implementing the callbacks,
 * custom functions, and game commands to the function tables are the
 * responsibility of the client code / dialogue runner pumping the instruction
 * queue is the responsibility of the client code / dialogue runner The VM has
 * built in json (de)serialization methods See the public interface / members
 * below, the base dialogue runner class in yarn_dialogue_runner.h, and the
 * included demo console dialogue runner program in demo.cpp for more
 */

#include <functional>
#include <iostream>
#include <random>
#include <stack>

#include <yarn_spinner.pb.h>

#ifdef YARN_SERIALIZATION_JSON
#include <json.hpp>
#endif

#define YarnException std::runtime_error

#define YARN_EXCEPTION(x)                                                      \
    if (settings.enableExceptions)                                             \
    {                                                                          \
        throw YarnException(x);                                                \
    }

std::ostream &operator<<(std::ostream &str, const Yarn::Operand &op);

namespace Yarn
{
class Operand;
class Node;
class Instruction;
class Program;

struct YarnVM
{
    /// settings passed into constructor of YarnVM
    struct Settings
    {
        std::mt19937::result_type randomSeed = std::mt19937::default_seed;
        bool enableExceptions = true;
    };

    /// information on line to run
    struct Line
    {
        std::string
            id; ///< unique identifier for the line - actual text and metadata
                ///< are stored in lines database object & retrieved from there
        std::vector<Yarn::Operand> substitutions;
    };

    /// Information on an option presented to the user
    struct Option
    {
        Line line; ///< line to display to the user for this option
        std::string
            destination; ///< destination to go to if this option is selected
        bool enabled;    ///< whether the option has a condition on it(in which
                      ///< case a value should be popped off the stack and used
                      ///< to signal the game that the option should be not
                      ///< available)
    };

    /// Variable stack used by the VM
    class Stack : public std::stack<Yarn::Operand>
    {
      public:
#ifdef YARN_SERIALIZATION_JSON
        nlohmann::json to_json() const;
        void from_json(const nlohmann::json &js);
#endif
    };

    typedef std::function<Yarn::Operand(YarnVM &yarn, int parameterCount)>
        YarnFunction;
    typedef std::vector<Option> OptionsList;

    /// Current State of the VM
    enum RunningState
    {
        RUNNING,
        STOPPED,
        AWAITING_INPUT,
        ASLEEP
    };

#define YARN_FUNC(x)                                                           \
    functions[x] = [](YarnVM & yarn, int parameters) -> Yarn::Operand

    /// User bindable callbacks available to the VM.
    struct YarnCallbacks
    {
        // -- mandatory implementation by caller --
        virtual void onRunLine(const YarnVM::Line &) = 0;
        virtual void onRunCommand(const std::string &) = 0;
        virtual void onPresentOptions(const OptionsList &) = 0;

        // -- additional helper callbacks, default no-op --

        virtual void onProgramStopped() {};
        virtual void onChangeNode(const Yarn::Node *fromNode,
                                  const Yarn::Node *toNode) {};
    };

    void setCallbacks(YarnCallbacks *callbacks) { this->callbacks = callbacks; }

    /// --- VM state members which are serializable! ---

    OptionsList currentOptionsList;

    Stack variableStack;

    std::unordered_map<std::string, Yarn::Operand> variableStorage;

    // https://stackoverflow.com/questions/27727012/c-stdmt19937-and-rng-state-save-load-portability
    // we want to be able to serialize / deserialize the rng state and continue
    // deterministically this means we can't use std::default_random_engine for
    // the generator since this is implementation defined.
    std::mt19937 generator;

    std::size_t instructionPointer = 0;

    RunningState runningState = STOPPED;

    Settings settings;

    long long time = 0;
    long long waitUntilTime = 0;

    std::string yarncFile;

    // --- the following members are not directly serializable but store some
    // derived properties

    const Yarn::Node *currentNode = nullptr;

    // --- The following members are NOT part of the serializable state! ---
    // c++ callbacks and function tables are to be populated directly by the
    // client / game code the program is derived from the yarnc filename in the
    // deserialization function

    std::unordered_map<std::string, YarnFunction> functions;
    YarnCallbacks *callbacks = nullptr;
    Yarn::Program program;

    // --- Public method interface below.  Called by your Dialogue Runner class
    // which owns this VM ---

    // default delegates to Settings constructor
    YarnVM() : YarnVM(Settings{}) {}
    YarnVM(const Settings &setts);

    bool loadNode(const std::string &node);

    bool loadProgram(const std::string &is);

    void setTime(
        long long timeIn); ///< for the built in "wait" command.  units
                           ///< are up to the dialogue runner and script

    void incrementTime(long long dt) { setTime(time + dt); }

    void waitUntil(long long t)
    {
        waitUntilTime = t;
        runningState = RunningState::ASLEEP;
    } ///< for the built in "wait" command.  units are up to the dialogue runner
      ///< and script

    void setWaitTime(long long t)
    {
        waitUntil(time + t);
    } ///< for the built in "wait" command.  units are up to the dialogue runner
      ///< and script

    void selectOption(const Option &option);

    void selectOption(int selection);

    void processInstruction(const Yarn::Instruction &instruction);

    const Yarn::Instruction &currentInstruction();

    void setInstruction(std::int32_t instruction);

    const Yarn::Instruction &advance(); ///< advance the instruction pointer and
                                        ///< return the next instruction

    unsigned int visitedCount(
        const std::string &node); ///< how many times has a node been
                                  ///< entered/exited during execution

#ifdef YARN_SERIALIZATION_JSON

    /// deserializes the static state of the VM from a json input.
    /// loads the node, which causes the node change callback to fire
    /// if we were in an awaiting input state, we fire the callback to present
    /// options
    void fromJS(const nlohmann::json &js);

    nlohmann::json toJS() const;

#endif

  protected: // internal helper methods
    void populateFuncs();

    std::string get_string_operand(const Yarn::Instruction &instruction,
                                   int index);

    bool get_bool_operand(const Yarn::Instruction &instruction, int index);

    float get_float_operand(const Yarn::Instruction &instruction, int index);
};
} // namespace Yarn
