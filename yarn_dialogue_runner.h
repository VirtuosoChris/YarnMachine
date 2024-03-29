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
            bool alwaysIgnoreMarkup = false;    ///< always ignore all markup and treat it as raw text
            bool nomarkup = false;              ///< is the built in nomarkup attribute toggled
            bool emitUnhandledMarkup = true;    ///< spits out markup with an unhandled / unknown attrib identifier as part of the line.  set to false to omit that text instead
        };

        const static std::string CLOSE_ALL_ATTRIB;

        Yarn::LineDatabase db;
        Yarn::YarnVM vm;
        Virtuoso::QuakeStyleConsole commands; ///< Use a c++ quake style console as a command parser / command provider

        // args are string view of line text, and markup attribute data
        typedef std::function<void(const std::string_view&, const Yarn::Markup::Attribute&)> AttribCallback;

        std::unordered_map<std::string, AttribCallback> markupCallbacks;

        std::string moduleName;

        Settings setts;

        YarnRunnerBase();

        virtual void onReceiveText(const std::string_view& s, bool eol = false) = 0;

        void onRunLine(const Yarn::YarnVM::Line& line) override;
        void onRunCommand(const std::string& command) override;

        void replace(const std::string& s, const std::string& repl, const char x = '%');

#ifdef YARN_SERIALIZATION_JSON
        virtual void save(const std::string& saveFile = "YarnVMSerialized.json");
        virtual void restore(const std::string& restoreFile = "YarnVMSerialized.json");
#endif

        /// -- loads yarn module with the module with path & name in 'module' argument, and attempts to transition to node named in 'startNode' argument
        /// This method will attempt to load the following files:
        /// <module> + ".yarnc"
        /// <module> + "-Lines.csv"
        /// <module> + "-Metadata.csv"
        /// So we don't load the .yarn script directly, but the output of the ysc.exe compiler, namely the compiled .yarnc,
        /// as well as the line database and metadata which are stored in csv files
        void loadModule(const std::string& module, const std::string& startNode = "Start");

        void processLine(const std::string_view& line, const Yarn::Markup::LineAttributes& attribs);

    private:

        void setAttribCallbacks(); // set built in attrib callbacks
        void loadModuleLineDB(const std::string& moduleName);
        static const std::string& findValue(const Yarn::Markup::Attribute& attrib);
    };
}

