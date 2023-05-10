#pragma once
#include <regex>
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

            void parseProperties(const std::string_view& x)
            {
                std::cout << "parsing properties out of " << x << std::endl;

                // regex captures the key-value pairs
                std::regex re2("([^\\s]+)\\s*=\\s*(\"([^\"]+)\"|[^\\s|\"]+)", std::regex_constants::ECMAScript | std::regex_constants::icase);

                auto words_begin = std::regex_iterator<std::string_view::iterator>(x.begin(), x.end(), re2);
                auto words_end = std::regex_iterator<std::string_view::iterator>();

                for (std::regex_iterator i = words_begin; i != words_end; ++i)
                {
                    int valueIndex = i->str(3).length() ? 3 : 2;

                    std::cout << "key value of " << i->str(1) << "::" << i->str(valueIndex) << std::endl;

                    properties[i->str(1)] = i->str(valueIndex);

                    if (name.length() == 0)
                    {
                        name = i->str(1);
                        //std::cout << "Defaulting attribute name to " << attr.name << std::endl;
                    }
                }
            }
        };

        struct LineAttributes
        {
            std::vector<Attribute> attribs;

            LineAttributes(const std::string_view& line)
            {
                parse(line);
            }


            void parseCharacter(const char* line)
            {
                std::regex re("([^:]+):[\\s]*", std::regex_constants::ECMAScript | std::regex_constants::icase);

                std::cmatch cm;
                if (std::regex_search(line, cm, re))
                {
                    //std::cout << cm.str(1) << std::endl;

                    this->attribs.push_back({});

                    auto& attr = this->attribs.back();

                    attr.type = Attribute::SELF_CLOSING;
                    attr.name = "character";
                    attr.properties["name"] = cm.str(1);
                    attr.length = cm.str(0).length();
                    attr.position = 0;

                    assert(cm.position(0) == 0);
                }

            }

            void parse(const std::string_view& line)
            {
                // -- regex reference :: https://cplusplus.com/reference/regex/ECMAScript/ --
                // since regexes are horrible you can test that they work here : https://regex101.com/

                // this regex finds the opening bracket, captures the attribute name, captures the entire properties string, and captures the closing bracket
                // we look for the attrib name at the beginning, any contiguous sequence of alnum

                parseCharacter(line.data());

                std::regex re("\\[[\\s]*([\\w|\\d]*)\\s*(=\\s*(?:\"(?:[^\"]+)\"|[^\\s|\"]+))?([^\\/\\]]*)(\\/?\\s*(\\w*)\\])", std::regex_constants::ECMAScript | std::regex_constants::icase);

                auto words_begin = std::regex_iterator<std::string_view::iterator>(line.begin(), line.end(), re);
                auto words_end = std::regex_iterator<std::string_view::iterator>();

                for (std::regex_iterator i = words_begin; i != words_end; ++i)
                {
                    //std::cout << "processing attrib section" << std::endl;

                    attribs.push_back(Attribute{});
                    Attribute& attr = attribs.back();

                    auto match = *i;

                    for (int i = 0; i < match.size(); i++)
                    {
                        std::cout << '\t' << match.str(i) << " which is captured at index " << match.position(i) << " and length " << match.str(i).length() << std::endl;
                    }

                    attr.position = match.position(0);
                    attr.length = match.str(0).length();

                    //std::cout << "Match size is " << match.size() << std::endl;

                    if ((match.size() > 1) && match.str(1).length())
                    {
                        attr.name = match.str(1);
                    }

                    // the optional shorthand one eg for [bounce=2] instead of [bounce bounce=2]
                    if ((match.size() > 2) && match.str(2).length())
                    {
                        assert(attr.name.size());

                        std::string tmp = attr.name + match.str(2);

                        attr.parseProperties(tmp);
                    }

                    //index 3 is the properties string
                    if ((match.size() > 3) && match.str(3).length())
                    {
                        int startPos = match.position(3);
                        int length = match.str(3).length();
                        std::string_view x(&line[startPos], length);

                        attr.parseProperties(x);
                    }

                    // index 4 is close tag
                    if (match.size() > 4)
                    {
                        if (match.str(4).length() == 1) // we just have a ] at the end, no /]
                        {
                            attr.type = attr.OPEN;

                            std::cout << "opening attrib " << attr.name << std::endl;
                        }
                        else if (match.str(4).length() > 1) // we have a / and maybe an attrib name before the ]
                        {
                            assert(attr.NONE == attr.type);

                            // index 5 is attrib we're closing
                            if (match.size() > 5 && (match.str(5).length()))
                            {
                                assert(attr.name.size() == 0);

                                attr.name = match.str(5);

                                // if there's no attribute named with a /, then we are closing all
                                //attr.type = attr.name.size() ? attr.CLOSE : attr.CLOSE_ALL;

                                attr.type = attr.CLOSE;

                                std::cout << "Closing attribute " << attr.name << std::endl;

                                assert(attr.properties.size() == 0);
                            }
                            else // here we have a / but no text before the ]
                            {
                                if (attr.name.size())
                                {
                                    attr.type = attr.SELF_CLOSING;
                                    std::cout << "self closing attribute " << attr.name << std::endl;
                                }
                                else
                                {
                                    attr.type = attr.CLOSE_ALL;
                                    std::cout << "Closing all attributes " << std::endl;

                                    assert(attr.properties.size() == 0);
                                }
                            }
                        }
                    }

                    assert(attr.type != attr.NONE);
                }
            }
        };
    }
}