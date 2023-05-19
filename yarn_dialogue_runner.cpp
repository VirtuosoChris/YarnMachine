#ifdef YARN_SERIALIZATION_JSON
#include <json.hpp>
#endif

#include <yarn_dialogue_runner.h>
#include <yarn_markup.h>
#include <yarn_spinner.pb.h>

const std::string Yarn::YarnRunnerBase::CLOSE_ALL_ATTRIB = "Internal.CloseAll";

std::string_view emit(const std::string_view& line, std::size_t begin, std::size_t end, bool trimwhitespace)
{
    assert(end > begin);

    if (trimwhitespace && std::isspace(line[begin]))
    {
        begin++;
    }

    return line.substr(begin, end - begin);
}

void Yarn::YarnRunnerBase::replace(const std::string& s, const std::string& repl, const char x)
{
    size_t cursor = 0;
    size_t pos;
    while ((pos = s.find(x, cursor)) != std::string::npos)
    {
        onReceiveText(std::string_view(&s[cursor], pos - cursor));
        onReceiveText(repl);
        cursor = pos + 1;
    }

    onReceiveText(std::string_view(&s[cursor], s.size() - cursor));
}

const std::string& Yarn::YarnRunnerBase::findValue(const Yarn::Markup::Attribute& attrib)
{
    auto it = attrib.properties.find("value");

    if (it != attrib.properties.end())
    {
        const std::string& value = it->second;
        return value;
    }
    else
    {
        throw YarnException("select attribute missing value property");
    }
}

void Yarn::YarnRunnerBase::setAttribCallbacks()
{
    this->markupCallbacks["nomarkup"] = [this](const std::string_view& line, const Yarn::Markup::Attribute& attrib)
    {
        this->setts.nomarkup = attrib.type == Yarn::Markup::Attribute::OPEN;
    };

    this->markupCallbacks["select"] = [this](const std::string_view& line, const Yarn::Markup::Attribute& attrib)
    {
        const std::string& value = findValue(attrib);

        auto it = attrib.properties.find(value);

        if (it != attrib.properties.end())
        {
            //str << it->second;
            replace(it->second, value, '%');
        }
        else
        {
            // using other as fallback like in the plurals
            auto it = attrib.properties.find("other");
            if (it != attrib.properties.end())
            {
                //str << it->second;

                replace(it->second, value, '%');
            }
            else
            {
                throw YarnException("Unable to resolve value for select markup");
            }
        }
    };

    this->markupCallbacks["plural"] = [this](const std::string_view& line, const Yarn::Markup::Attribute& attrib)
    {
        const std::string& value = findValue(attrib);

        auto it = attrib.properties.find(Yarn::Markup::getCardinalPluralClass(value));

        if (it != attrib.properties.end())
        {
            replace(it->second, value, '%');
        }
        else
        {
            throw YarnException("Unable to resolve value for plural markup");
        }
    };

    this->markupCallbacks["ordinal"] = [this](const std::string_view& line, const Yarn::Markup::Attribute& attrib)
    {
        const std::string& value = findValue(attrib);

        auto it = attrib.properties.find(Yarn::Markup::getOrdinalPluralClass(value));

        if (it != attrib.properties.end())
        {
            //str << it->second;

            replace(it->second, value, '%');
        }
        else
        {
            throw YarnException("Unable to resolve value for ordinal markup");
        }
    };
}

Yarn::YarnRunnerBase::YarnRunnerBase()
{
    vm.setCallbacks(this);

    setAttribCallbacks();
}

void Yarn::YarnRunnerBase::processLine(const std::string_view& line, const Yarn::Markup::LineAttributes& attribs)
{
    std::size_t cursorIndex = 0;
    auto nextAttrib = attribs.attribs.begin();

    // inline properties for processing the line markup:

    // If a self - closing attribute has white - space before it, or it's at the start of the line, then it will trim a single whitespace after it. This means that the following text produces a plain text of "A B":
    //  A [wave / ] B
    //  If you don't want to trim whitespace, add a property trimwhitespace, set to false:
    //  A [wave trimwhitespace = false / ] B
    //  // (produces "A  B")
    bool trimwhitespace = false; //

    // nomarkup / nomarkup
    // [nomarkup] Here's a big ol'[bunch of] characters, filled [[]] with square [[]brackets![/ nomarkup]
    // bool nomarkup = false;

    while (cursorIndex < line.length())
    {
        if (nextAttrib == attribs.attribs.end())
        {
            onReceiveText(emit(line, cursorIndex, line.size(), trimwhitespace));
            cursorIndex = line.size();

            continue;
        }

        // determine if we should enable trim white space
        bool selfClosing = (nextAttrib->type == Yarn::Markup::Attribute::SELF_CLOSING);
        bool startOfLine = nextAttrib->position == 0;

        // this gets reset for each attribute, so it'll apply to the next (or not) and be consumed
        trimwhitespace = selfClosing && (startOfLine || std::isspace(line[nextAttrib->position - 1]));


        // check for trimwhitespace override
        std::unordered_map<std::string, std::string>::const_iterator it = nextAttrib->properties.find("trimwhitespace");
        if (it != nextAttrib->properties.end())
        {
            if (it->second == "false")
            {
                trimwhitespace = false;
            }
        }
        else // make plurals, ordinals, and select default to trimwhitespace if not otherwise specified for consistency with examples
        {
            if ((nextAttrib->name == "select") || (nextAttrib->name == "plural") || (nextAttrib->name == "ordinal"))
            {
                trimwhitespace = false;
            }
        }

        const bool isCloseAll = nextAttrib->type == Yarn::Markup::Attribute::CLOSE_ALL;

        // peek for close all attribute and turn off nomarkup
        if (isCloseAll)
        {
            setts.nomarkup = false;
        }

        if ((nextAttrib->type == Yarn::Markup::Attribute::CLOSE) && (nextAttrib->name == "nomarkup"))
        {
            setts.nomarkup = false;
        }

        // we have an attribute to emit
        {
            std::size_t al = nextAttrib->length;
            std::size_t ap = nextAttrib->position;

            if (ap - cursorIndex) // emit unmarked characters up to the beginning of the attribute
            {
                onReceiveText(emit(line, cursorIndex, ap, trimwhitespace));
            }

            assert(al);

            if (al)
            {
                auto it = isCloseAll ? markupCallbacks.find(CLOSE_ALL_ATTRIB) : markupCallbacks.find(nextAttrib->name);

                if ((!setts.nomarkup) && it != markupCallbacks.end())
                {
                    it->second(line, *nextAttrib);
                }
                else if (setts.emitUnhandledMarkup || setts.nomarkup)
                {
                    // -- unhandled markup attrib name or disabled markup : just emit the text --
                    onReceiveText(emit(line, ap, ap + al, false));
                }
            }

            // handle attrib;
            assert((al + ap) > cursorIndex);
            cursorIndex = al + ap; // advance cursor to after the attribute we just handled

            nextAttrib++;
        }
    }

    onReceiveText("", true);
}


std::string make_substitutions(const std::string_view& sv, const std::vector<Yarn::Operand>& substitutions)
{
    std::stringstream sstr;
    const int subsRemaining = substitutions.size();

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

            const auto& sub = substitutions[subsRemaining - arg - 1];

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

    return sstr.str();
}

void Yarn::YarnRunnerBase::loadModule(const std::string& mod, const std::string& startNode)
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

void Yarn::YarnRunnerBase::onRunLine(const Yarn::YarnVM::Line& line)
{
    std::string lineS;
    if (!line.substitutions.size())
    {
        lineS = db.lines[line.id].text;
    }
    else
    {
        lineS = make_substitutions(db.lines[line.id].text, line.substitutions);
    }

    if (setts.alwaysIgnoreMarkup)
    {
        onReceiveText(lineS);
    }
    else
    {
        // parse attributes from line text
        Yarn::Markup::LineAttributes attr(lineS);
        processLine(lineS, attr);
    }
}

void Yarn::YarnRunnerBase::onRunCommand(const std::string& command)
{
    this->commands.commandExecute(command, std::cout);
}

void Yarn::YarnRunnerBase::loadModuleLineDB(const std::string& moduleName)
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

#ifdef YARN_SERIALIZATION_JSON
void Yarn::YarnRunnerBase::save(const std::string& saveFile)
{
    nlohmann::json serialized;

    serialized["vm"] = vm.toJS();
    serialized["moduleName"] = moduleName;

    std::ofstream outJS(saveFile);

    outJS << std::setw(4) << serialized;

    outJS.close();
}

void Yarn::YarnRunnerBase::restore(const std::string& restoreFile)
{
    std::ifstream inJS(restoreFile);

    nlohmann::json js = nlohmann::json::parse(inJS);

    loadModuleLineDB(js["moduleName"].get<std::string>());

    vm.fromJS(js["vm"]);

    inJS.close();
}

#endif

