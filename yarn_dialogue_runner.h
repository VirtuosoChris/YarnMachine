#pragma once

/**
 * @file yarn_dialogue_runner.h
 *
 * @brief Yarn Spinner C++ Base Dialogue Runner Class
 *
 * @author Christopher Pugh
 * Contact: chris@virtuosoengine.com
 *
 * Usage:
 * Inherit from this class and override pure virtual YarnCallback 
 * virtual void onPresentOptions(const OptionsList&) = 0;
 * as well as
 * virtual void onReceiveText(const std::string_view& s) = 0; method for when the Yarn VM wants to display raw text
 * add whatever markupCallbacks for markup processing you need beyond the built in ones by creating std::function AttribCallback's and
 * adding them to markupCallbacks lookup table
 *
 * At runtime, load a Yarn module by path/module name by calling loadModule()
 * (de)serialize with save / restore method
 *
 * This class handles creating the VM, loading the line database, parsing and executing commands, running lines, and more.
 * Access the quake style console member "commands" to add custom commands.
 * Access the vm member to add custom functions
 */

#include <string>
#include <yarn_vm.h>

#include <QuakeStyleConsole.h>
#include <yarn_line_database.h>
#include <yarn_markup.h>

namespace Yarn
{
    struct YarnRunnerBase : public Yarn::YarnVM::YarnCallbacks
    {
        struct Settings
        {
            bool ignoreAllMarkup = false;
        };

        const static std::string CLOSE_ALL_ATTRIB;

        Yarn::LineDatabase db;
        Yarn::YarnVM vm;
        Virtuoso::QuakeStyleConsole commands; ///< Use a c++ quake style console as a command parser / command provider

        // args are string stream to write output line text to, string view of line text, and markup attribute data
        typedef std::function<void(std::ostream&, const std::string_view&, const Yarn::Markup::Attribute&)> AttribCallback;

        std::unordered_map<std::string, AttribCallback> markupCallbacks;

        std::string moduleName;

        Settings setts;

        YarnRunnerBase();

        virtual void onReceiveText(const std::string_view& s) = 0;

        void onRunLine(const Yarn::YarnVM::Line& line) override;
        void onRunCommand(const std::string& command) override;

#ifdef YARN_SERIALIZATION_JSON
        virtual void save(const std::string& saveFile = "YarnVMSerialized.json");
        virtual void restore(const std::string& restoreFile = "YarnVMSerialized.json");
#endif

        void loadModule(const std::string& mod, const std::string& startNode = "Start");
        void loadModuleLineDB(const std::string& moduleName);

        void processLine(const std::string_view& line, const Yarn::Markup::LineAttributes& attribs);

        void setAttribCallbacks(); // set built in attrib callbacks

    private:

        static const std::string& findValue(const Yarn::Markup::Attribute& attrib);
    };
}

