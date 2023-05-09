#include "yarn_util.h"

#include <sstream>
#include "yarn.h"

using namespace Yarn;

namespace Yarn
{
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
}
