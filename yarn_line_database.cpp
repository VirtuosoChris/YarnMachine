#include <yarn_line_database.h>

#include <csv.hpp>

void Yarn::LineDatabase::loadMetadata(const std::string_view& csvFile)
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

void Yarn::LineDatabase::loadLines(const std::string_view& csvFile)
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

