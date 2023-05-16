#pragma once

/**
 * @file yarn_markup.h
 *
 * @brief Yarn Spinner C++ methods and data structures for extracting markup / attributes from a Yarn dialogue line
 *
 * @author Christopher Pugh
 * Contact: chris@virtuosoengine.com
 *
 * Used by the base dialogue runner in yarn_dialogue_runner.h or your own custom Yarn Dialogue runner
 */

#include <string>
#include <unordered_map>
#include <vector>
#include <cassert>

namespace Yarn
{
    namespace Markup
    {
        struct Attribute
        {
            enum AttribType { NONE = 0, OPEN = 1, CLOSE = 2, SELF_CLOSING = 3, CLOSE_ALL = 4 };

            std::string name;
            std::unordered_map<std::string, std::string> properties;
            AttribType type = NONE;

            std::size_t position = 0;
            std::size_t length = 0;

            Attribute()
            {
            }

            void parseProperties(const std::string_view& x);
        };

        struct LineAttributes
        {
            std::vector<Attribute> attribs;

            LineAttributes(const std::string_view& line)
            {
                parse(line);
            }

            LineAttributes()
            {
            }

            void parseCharacter(const char* line);

            void parse(const std::string_view& line);
        };

        std::string getCardinalPluralClass(int value);

        std::string getOrdinalPluralClass(int value);

        inline std::string getCardinalPluralClass(const std::string& val)
        {
            return getCardinalPluralClass(std::stoi(val));;
        }

        inline std::string getOrdinalPluralClass(const std::string& val)
        {
            return getOrdinalPluralClass(std::stoi(val));
        }
    }
}