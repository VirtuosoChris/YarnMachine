#include <chrono>
#include <yarn_dialogue_runner.h>

void beep(int count)
{
    for (int i = 0; i < count; i++)
    {
        std::cerr << "Beep!\a\n";
    }
}

//WhAt DoEs ThIs FuNcTiOn Do?
std::string makeSarcastic(const std::string_view& in)
{
    std::string rval;
    rval.reserve(in.length());

    for (int i = 0; i < in.length(); i++)
    {
        if (std::isalpha(in[i]) && (i % 2))
        {
            rval.push_back(std::toupper(in[i]));
            continue;
        }

        rval.push_back(in[i]);
    }

    return rval;
}

/// A simple Yarn Dialogue Runner example that uses the console / cin / cout
/// This shows the intended usage of this library.
/// There are a lot of things that will be implementation dependent in your game / engine
/// For example, dialogue display / formatting, control flow, command implementations, time, etc.
/// So the intended usage is to have a runner class that inherits from YarnRunnerBase and:
/// - handles input
/// - processes and displays output
/// - controls the "loop" of when the VM runs
/// - make c++ std::function bindings for (non-built-in) functions and vm callbacks
/// - handle markup properties
struct YarnRunnerConsole : public Yarn::YarnRunnerBase
{
    bool sarcasm = false;

    YarnRunnerConsole()
    {
        setCommands();

        this->markupCallbacks[CLOSE_ALL_ATTRIB] = 
            [this](const std::string_view&, const Yarn::Markup::Attribute&)
        {
            // close all markup callback
            this->sarcasm = false;
        };

        this->markupCallbacks["sarcasm"] =
            [this](const std::string_view&, const Yarn::Markup::Attribute& attr)
        {
            // sarcastic markup
            this->sarcasm = (attr.type == Yarn::Markup::Attribute::OPEN);
        };

    }

    /// - this doesn't have to even be a member.  you could just access the command table member anywhere in your code and change
    /// the bound commands however you like
    void setCommands()
    {
        // 2 built in commands in Yarn specification : wait, and stop
        // stop is already handled by the compiler.
        // so we implement wait.
        // we also implement a beep command that takes a single integer argument for the number of times we beep.

        commands.bindCommand("beep", beep, "usage : beep <count>. Beep beep!");
        commands.bindMemberCommand("wait", vm, &Yarn::YarnVM::setWaitTime, "usage: wait <time>.  current time and time units determined by yarn dialogue runner.");
    }

    /// callback for when the Yarn runtime wants to present raw text
    void onReceiveText(const std::string_view& s, bool eol = false) override
    {
        if (sarcasm)
        {
            std::cout << makeSarcastic(s);// << std::endl;
        }
        else
        {
            std::cout << s;// << std::endl;
        }

        if (eol) std::cout << std::endl;
    }

    /// yarn vm callback for when the VM wants to present options to the player
    void onPresentOptions(const Yarn::YarnVM::OptionsList& opts) override
    {
        for (int i = 0; i < opts.size(); i++)
        {
            if (opts[i].enabled)
            {
                std::cout << '\t' << (i + 1) << ") ";
                onRunLine(opts[i].line);
            }
        }

        int lineIndex = 0;

        /// -- should actually do input string validation.  won't be using a cin input irl, so nbd for now
        std::cin >> lineIndex;

        lineIndex -= 1; // zero offset on cin

        vm.selectOption(lineIndex);
    }

    /// handle the main loop, processing instructions, updating the time, etc.
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
}

