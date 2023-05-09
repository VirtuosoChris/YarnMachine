#pragma once

#include <string>
#include <vector>

namespace Yarn
{
    class Operand;

    std::string make_substitutions(const std::string_view& lineText, const std::vector<Yarn::Operand>& substitutions);
}
