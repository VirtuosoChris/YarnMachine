#include "yarn.h"
#include "yarn_line_database.h"

#include <iostream>
#include <fstream>
#include <chrono>

#include <QuakeStyleConsole.h>


void beep(int count)
{
    for (int i = 0; i < count; i++)
    {
        std::cerr << '\a';
    }
}

/// A simple Yarn Dialogue Runner example that uses the console / cin / cout
/// This shows the intended usage of this library.
/// There are a lot of things that will be implementation dependent in your game / engine
/// For example, dialogue display / formatting, control flow, command implementations, time, etc.
/// So the intended usage is to have a runner class that:
/// - Creates a Yarn VM that runs the .yarnc scripts, and signals a callback with commands, options, and line id's to run
/// - Creates and populates a line database that is queried whenever the Yarn VM signals that a line of dialogue should run
/// - handles input, processes and displays output, runs commands, and controls the "loop" of when the VM runs
/// - controls serialization and deserialization of the VM state using the provided methods
/// - make c++ std::function bindings for (non-built-in) functions and vm callbacks
struct YarnRunnerConsole
{
    Yarn::LineDatabase db;
    Yarn::YarnVM vm;
    Virtuoso::QuakeStyleConsole commands; ///< Use a c++ quake style console as a command parser / command provider

    YarnRunnerConsole()
    {
        setCallbacks();
        setCommands();
    }

    void setCommands()
    {
        // 2 built in commands in Yarn specification : wait, and stop
        // stop is already handled by the compiler.
        // so we implement wait.
        // we also implement a beep command that takes a single integer argument for the number of times we beep.

        commands.bindCommand("beep", beep, "usage : beep <count>. Beep beep!");
        commands.bindMemberCommand("wait", vm, &Yarn::YarnVM::setWaitTime, "usage: wait <time>.  current time and time units determined by yarn dialogue runner.");
    }

#ifdef YARN_SERIALIZATION_JSON
    void save(const std::string& saveFile = "YarnVMSerialized.json")
    {
        nlohmann::json serialized = vm.toJS();

        std::ofstream outJS(saveFile);

        outJS << std::setw(4) << serialized;

        outJS.close();

        ///\todo
    }

    void restore(const std::string& restoreFile = "YarnVMSerialized.json")
    {
        ///\todo
    }

#endif

    void loadModule(const std::string& moduleName)
    {
        const std::string testLinesCSV = moduleName + "-Lines.csv";
        const std::string testMetaCSV = moduleName + "-Metadata.csv";
        const std::string yarncFile = moduleName + ".yarnc";

        db.loadLines(testLinesCSV);
        db.loadMetadata(testMetaCSV);

#if _DEBUG
        std::cout << "Loading lines from : " << testLinesCSV << std::endl;
        std::cout << "Line database total lines : " << db.lineCount() << std::endl;
        std::cout << "Line database size (bytes) : " << db.sizeBytes() << std::endl;
        std::cout << "Time spent in parsing (ms) : " << db.parsingTime << std::endl;
#endif

        std::ifstream file(yarncFile, std::ios::binary | std::ios::in);

        bool fileOpen = file.is_open();

        std::clog << "file open? : " << fileOpen << std::endl;

        vm.loadProgram(file);

        //m.printNodes();

        auto it = vm.program.nodes().begin();
        if ((it = vm.program.nodes().find("Start")) != vm.program.nodes().end())
        {
            const Yarn::Node& startnode = it->second;

            vm.loadNode("Start");
        }
    }

    void setCallbacks()
    {
        vm.callbacks.onCommand = [this](const std::string& command)
        {
            std::cout << "command : " << command << std::endl;

            this->commands.commandExecute(std::cin, std::cout);
        };

        vm.callbacks.onLine = [this](const Yarn::YarnVM::Line& line)
        {
            //
#ifdef _WIN32
        //getch();
#endif

#if 1

            std::cout << "Running line " << line.id;

            if (db.tags[line.id].size())
            {
                std::cout << " With tags : \n";

                for (auto& tag : db.tags[line.id])
                {
                    std::cout << '\t' << tag << '\n';
                }
            }
            std::cout << std::endl;
#endif

            std::stringstream sstr;

            int subsRemaining = line.substitutions.size();

            if (!subsRemaining)
            {
                std::cout << db.lines[line.id].text << std::endl;
            }
            else
            {
                ///\todo major audit of this.

                std::string_view sv(db.lines[line.id].text);

                for (int i = 0; i < sv.size(); i++)
                {
                    if (sv[i] == '{')
                    {
                        int arg = -1;
                        i++;
                        std::istringstream is(sv.data() + i);
                        is >> arg;

                        while (isdigit(sv[i]))i++;

                        assert(sv[i] == '}');

                        const auto& sub = line.substitutions[subsRemaining - arg - 1];

                        if (sub.has_bool_value())
                        {
                            sstr << sub.bool_value();
                        }
                        else if (sub.has_float_value())
                        {
                            sstr << sub.float_value();
                        }
                        else if (sub.has_string_value())
                        {
                            sstr << sub.string_value();
                        }
                        else
                        {
                            assert(0 && "bad variable");
                        }
                    }
                    else
                    {
                        sstr << sv[i];
                    }
                }

                std::cout << sstr.str() << std::endl;
            }
        };

#if 1
        vm.callbacks.onChangeNode = [this]()
        {
            std::cout << "Entering node : " << vm.currentNode->name();
            if (vm.currentNode->tags_size())
            {
                std::cout << " with tags:\n";

                for (auto& x : this->vm.currentNode->tags())
                {
                    std::cout << '\t' << x << '\n';
                }
            }

            if (vm.currentNode->headers_size())
            {
                std::cout << ", with headers\n";

                for (auto& x : vm.currentNode->headers())
                {
                    std::cout << '\t' << x.key() << ' ' << x.value() << '\n';
                }
            }
            std::cout << std::endl;
        };
#endif

        vm.callbacks.onShowOptions = [this](const Yarn::YarnVM::OptionsList& opts)
        {
            for (int i = 0; i < opts.size(); i++)
            {
                ///\todo substitutions
                const std::string& lineID = opts[i].line.id;
                std::cout << '\t' << (i + 1) << ") " << db.lines[lineID].text << std::endl;

                ///\todo input string validation
            }

            int lineIndex = 0;

            std::cin >> lineIndex;

            lineIndex -= 1; // zero offset on cin

            const Yarn::YarnVM::Option& line = opts[lineIndex];

            Yarn::Operand op;
            op.set_string_value(line.destination);

            vm.variableStack.push(op);

            ///\todo this doesn't belong here!
            vm.currentOptionsList.clear();
        };
    }

    void loop()
    {
        const Yarn::Instruction& inst = vm.currentInstruction();
        vm.processInstruction(inst);

        while (true)
        {
            // update the time

            switch (vm.runningState)
            {
            case Yarn::YarnVM::RUNNING:
                vm.processInstruction(vm.currentInstruction());
                break;
            case Yarn::YarnVM::ASLEEP:
                break;
            default:
                return;
            }
        }

    }

};

/// write all variables, types, and values to an ostream as key:value pairs, one variable per line
std::ostream& logVariables(std::ostream& str, const Yarn::YarnVM& vm)
{
    for (auto it = vm.variableStorage.begin(); it != vm.variableStorage.end(); it++)
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


int main(void)
{
    const std::string TestFolder = "test";
    const std::string TestFilePrefix = "/test9";
    const std::string testFile = TestFolder + TestFilePrefix;

    YarnRunnerConsole yarn;

    yarn.loadModule(testFile);

    // do the main loop and run the program.
    yarn.loop();

    // dump variables to the console after the script runs
    logVariables(std::cout, yarn.vm);

}

