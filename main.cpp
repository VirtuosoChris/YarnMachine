#include "yarn.h"
#include "yarn_line_database.h"

#include <iostream>
#include <fstream>
#include <chrono>

#include <QuakeStyleConsole.h>

#include "yarn_util.h"


void beep(int count)
{
    for (int i = 0; i < count; i++)
    {
        std::cerr << "Beep!\a\n";
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

    std::string moduleName;

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
        nlohmann::json serialized;

        serialized["vm"] = vm.toJS();
        serialized["moduleName"] = moduleName;

        std::ofstream outJS(saveFile);

        outJS << std::setw(4) << serialized;

        outJS.close();
    }

    void restore(const std::string& restoreFile = "YarnVMSerialized.json")
    {
        std::ifstream inJS(restoreFile);

        nlohmann::json js = nlohmann::json::parse(inJS);

        loadModuleLineDB(js["moduleName"].get<std::string>());

        vm.fromJS(js["vm"]);

        inJS.close();
    }

#endif

    void showLine(const Yarn::YarnVM::Line& line)
    {
        if (!line.substitutions.size())
        {
            std::cout << db.lines[line.id].text << std::endl;
        }
        else
        {
            std::cout << Yarn::make_substitutions(db.lines[line.id].text, line.substitutions) << std::endl;
        }
    }

    void loadModuleLineDB(const std::string& moduleName)
    {
        const std::string testLinesCSV = moduleName + "-Lines.csv";
        const std::string testMetaCSV = moduleName + "-Metadata.csv";

        db.loadLines(testLinesCSV);
        db.loadMetadata(testMetaCSV);

#if _DEBUG
        std::cout << "Loading lines from : " << testLinesCSV << std::endl;
        std::cout << "Line database total lines : " << db.lineCount() << std::endl;
        std::cout << "Line database size (bytes) : " << db.sizeBytes() << std::endl;
        std::cout << "Time spent in parsing (ms) : " << db.parsingTime << std::endl;
#endif
    }

    void loadModule(const std::string& mod, const std::string& startNode = "Start")
    {
        moduleName = mod;

        const std::string yarncFile = moduleName + ".yarnc";

        loadModuleLineDB(mod);
        vm.loadProgram(yarncFile);

        // find the start node
        auto it = vm.program.nodes().begin();
        if ((it = vm.program.nodes().find(startNode)) != vm.program.nodes().end())
        {
            const Yarn::Node& startnode = it->second;

            vm.loadNode(startNode);
        }
    }

    void setCallbacks()
    {
        vm.callbacks.onCommand = [this](const std::string& command)
        {
            std::cout << "command : " << command << std::endl;

            this->commands.commandExecute(command, std::cout);
        };

        vm.callbacks.onLine = [this](const Yarn::YarnVM::Line& line)
        {
#if _DEBUG

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

            showLine(line);
        };

#if _DEBUG

        vm.callbacks.onChangeNode = [this]()
        {
            assert(vm.currentNode);
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
                if (opts[i].enabled)
                {
                    std::cout << '\t' << (i + 1) << ") ";
                    showLine(opts[i].line);
                }

                /// -- should actually do input string validation.  won't be using a cin input irl, so nbd for now
            }

            int lineIndex = 0;

            std::cin >> lineIndex;

            lineIndex -= 1; // zero offset on cin

            vm.selectOption(lineIndex);
        };
    }

    void loop()
    {
        const Yarn::Instruction& inst = vm.currentInstruction();
        vm.processInstruction(inst);

        auto time = std::chrono::steady_clock::now();

        while (true)
        {
            // update the time
            auto time2 = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time);
            
            long long dt = duration.count();

            if (dt)
            {
                vm.incrementTime(dt);
                time = time2;
            }

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


int main(int argc, char* argv[])
{
    std::string testFile;

    if (argc > 1)
    {
        std::cout << "Loading Yarn module " << argv[1] << std::endl;
        testFile = argv[1];
    }
    else
    {
        std::cout << "Type name of module to run : " << std::endl;
        std::cin >> testFile;
    }

    YarnRunnerConsole yarn;

    yarn.loadModule(testFile);

    // do the main loop and run the program.
    yarn.loop();

    // dump variables to the console after the script runs
    logVariables(std::cout, yarn.vm);

}

