#pragma once

/**
 * @file yarn_line_database.h
 *
 * @brief Yarn Spinner C++ Class for Loading / Providing Dialog Lines and Metadata
 *
 * @author Christopher Pugh
 * Contact: chris@virtuosoengine.com
 *
 * Usage:
 * Create an instance of LineDatabase in your Yarn Dialogue Runner
 * To initialize, you need the CSV file generated by the Yarn Compiler containing the line data, as well as optionally the metadata CSV file
 * eg:
 * LineDatabase db;
 * db.loadLines("Test-Lines.csv");
 * db.loadMetadata("Test-Metadata.csv");
 * or:
 * db.load("Test-Lines.csv", "Test-Metadata.csv");
 *
 * To use the data in your Yarn Dialogue Runner, simply use a LineID string as a key index into the lines or tags member maps.  Eg:
 * std::cout << db.lines[lineID].text << std::endl;
 * 
 * if (db.tags.find("sarcastic") != db.tags.end())
 * {
 *      // display with sarcastic font
 * }
 */

#include <csv.hpp>

namespace Yarn
{

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
        std::unordered_map<LineID, std::unordered_set<LineTag> > tags;
        long long parsingTime = 0;

        /*
        enum MarkupFlags {RAW_TEXT = 0, NO_MARKUP = 0x1};

        std::string fetchLine(const LineID& lineID, MarkupFlags flags = RAW_TEXT)
        {
            if (RAW_TEXT == flags)
            {
                return lines[lineID].text;
            }
        }
        */

        uint64_t lineCount() const { return lines.size(); }

        uint64_t sizeBytes() const
        {
            uint64_t rval = 0;
            for (const auto& [key, value] : lines)
            {
                rval += value.sizeBytes();
                rval += key.length() + 1 + sizeof(key); // got to include the size of the "key" strings (lineID's) too.
            }
            return rval;
        }

        LineDatabase() { }

        void load(const std::string_view& lineCSVFile, const std::string_view& metaCSVFile)
        {
            loadLines(lineCSVFile);
            loadMetadata(metaCSVFile);
        }

        void loadMetadata(const std::string_view& csvFile)
        {
            const int YARN_TAGS_COLUMN_INDEX = 3;

            auto start = std::chrono::high_resolution_clock::now();

            csv::CSVFormat format;
            format.variable_columns(true);
            format.variable_columns(csv::VariableColumnPolicy::KEEP);

            csv::CSVReader reader(csvFile, format);

            for (const csv::CSVRow& row : reader)
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

            for (const csv::CSVRow& row : reader)
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
}
