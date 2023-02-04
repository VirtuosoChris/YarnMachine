#include "yarn.h"
#include "./depends/csv.hpp"

#include <iostream>
#include <fstream>

#include <chrono>

#ifdef _WIN32
#include <conio.h>
#endif

const int YARN_TAGS_COLUMN_INDEX = 3;

using namespace csv;

struct LineData
{
    std::string id;
    std::string text;
    std::string file;
    std::string node;
    int lineNumber = 0;

    uint64_t sizeBytes() const
    {
        return id.size() + sizeof(id)
            + text.size() + sizeof(text)
            + file.size() + sizeof(file)
            + node.size() + sizeof(node)
            + sizeof(lineNumber);
    }
};

typedef std::string LineID;
typedef std::string LineTag;

struct LineDatabase
{
    std::unordered_map<LineID, LineData> lines;
    long long parsingTime = 0;

    std::unordered_map<LineID, std::unordered_set<LineTag> > tags;

    uint64_t lineCount()
    {
        return lines.size();
    }

    uint64_t sizeBytes()
    {
        uint64_t rval = 0;
        for (const auto& [key, value] : lines)
        {
            rval += value.sizeBytes();
        }
        return rval;
    }

    LineDatabase()
    {
    }

    LineDatabase(const std::string_view& csvFile)
    {
        loadLines(csvFile);
    }

    void loadMetadata(const std::string_view& csvFile)
    {
        auto start = std::chrono::high_resolution_clock::now();

        csv::CSVFormat format;
        format.variable_columns(true);
        format.variable_columns(VariableColumnPolicy::KEEP);

        csv::CSVReader reader(csvFile, format);

        for (const CSVRow& row : reader)
        {
            std::string id = row["id"].get();

            for (int i = YARN_TAGS_COLUMN_INDEX; i < row.size(); i++)
            {
                tags[id].insert(row[i].get());
            }
        }

        auto stop = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        parsingTime += duration.count();
    }

    void loadLines(const std::string_view& csvFile)
    {
        auto start = std::chrono::high_resolution_clock::now();

        csv::CSVReader reader(csvFile);

        for (const CSVRow& row : reader)
        {
            int lineNumber = 0;

            if (row["lineNumber"].is_int())
            {
                lineNumber = row["lineNumber"].get<int>();
            }
            else
            {
                assert(0);
            }

            std::string id = row["id"].get();

            lines[id] = 
            {
                id,
                row["text"].get(),
                row["file"].get(),
                row["node"].get(),
                lineNumber
            };
        }

        auto stop = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        parsingTime += duration.count();
    }
};


int main(void)
{
    LineDatabase db;

    const std::string testLinesCSV = "test/test-Lines.csv";
    const std::string testMetaCSV = "test/test-Metadata.csv";

    db.loadLines(testLinesCSV);
    db.loadMetadata(testMetaCSV);

#if 0
    std::cout << "Loading lines from : " << testLinesCSV << std::endl;
    std::cout << "Line database total lines : " << db.lineCount() << std::endl;
    std::cout << "Line database size (bytes) : " << db.sizeBytes() << std::endl;
    std::cout << "Time spent in parsing (ms) : " << db.parsingTime << std::endl;
#endif

    YarnMachine m;

    m.callbacks.onLine = [&db](const std::string& line)
    {
        std::cout << db.lines[line].text << std::endl;
#ifdef _WIN32
        getch();
#endif
    };

    m.callbacks.onShowOptions = [&db, &m](const YarnMachine::OptionsList& opts)
    {
        for (int i = 0; i < opts.size(); i++)
        {
            ///\todo substitutions
            const std::string& lineID = opts[i].line.id;
            std::cout << '\t' << (i+1) << ") " << db.lines[lineID].text << std::endl;

            ///\todo input string validation
        }

        int lineIndex = 0;

        std::cin >> lineIndex;

        lineIndex -= 1; // zero offset on cin

        const YarnMachine::Option& line = opts[lineIndex];

        Yarn::Operand op;
        op.set_string_value(line.destination);

        m.variableStack.push(op);

        ///\todo this doesn't belong here!
        m.currentOptionsList.clear();
    };

    std::ifstream file("./test/test.yarnc", std::ios::binary | std::ios::in);

    bool fileOpen = file.is_open();

    std::clog << "file open? : " << fileOpen << std::endl;

    m.loadProgram(file);

    //m.printNodes();

    auto it = m.program.nodes().begin();
    if (( it = m.program.nodes().find("Start")) != m.program.nodes().end())
    {
        const Yarn::Node& startnode = it->second;

        m.loadNode("Start");
    }

    const Yarn::Instruction& inst = m.programState.currentInstruction();
    m.processInstruction(inst);

    while (m.programState.runningState == YarnMachine::ProgramState::RUNNING)
    {
        m.processInstruction(m.programState.currentInstruction());
    }
}

