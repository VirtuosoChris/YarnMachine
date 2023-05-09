#include "yarn.h"

using namespace Yarn;

void YarnVM::processInstruction(const Yarn::Instruction& instruction)
{
    if (!currentNode)
    {
        YARN_EXCEPTION("Current node is nullptr in processInstruction()");
    }

    auto op = instruction.opcode();

    switch (op)
    {
    case Yarn::Instruction_OpCode_JUMP_TO:
    {
        const std::string& jumpLabel = get_string_operand(instruction, 0);

        if (currentNode->labels().find(jumpLabel) == currentNode->labels().end())
        {
            YARN_EXCEPTION("JUMP_TO : Jump label doesn't exist in current node");
        }

        std::int32_t translatedLabel = currentNode->labels().at(jumpLabel);

        setInstruction(translatedLabel);

        return;
    }
    break;
    case Yarn::Instruction_OpCode_JUMP:
    {
        const Yarn::Operand& stackTop = variableStack.top();

        if ((variableStack.size() == 0) || !stackTop.has_string_value())
        {
            YARN_EXCEPTION("Stack top doesn't have a string value in JUMP instruction");
        }

        const std::string& jumpLoc = stackTop.string_value();

        std::int32_t translatedLabel = currentNode->labels().at(jumpLoc);

        setInstruction(translatedLabel);

        return;
    }
    break;
    case Yarn::Instruction_OpCode_RUN_LINE:
    {
        const std::string& lineID = get_string_operand(instruction, 0);

        int substitutions = get_float_operand(instruction, 1);

        YarnVM::Line l;
        l.id = lineID;

        l.substitutions.reserve(substitutions);

        for (int i = 0; i < substitutions; i++)
        {
            l.substitutions.push_back(variableStack.top());
            variableStack.pop();
        }

        callbacks.onLine(l);
    }
    break;
    case Yarn::Instruction_OpCode_RUN_COMMAND:
    {
        const std::string& commandText = get_string_operand(instruction, 0);
        callbacks.onCommand(commandText);
    }
    break;
    case Yarn::Instruction_OpCode_ADD_OPTION:
    {
        // Adds an entry to the option list (see ShowOptions).
        // - opA = string: string ID for option to add
        // - opB = string: destination to go to if this option is selected
        // - opC = number: number of expressions on the stack to insert
        //   into the line
        // - opD = bool: whether the option has a condition on it (in which
        //   case a value should be popped off the stack and used to signal
        //   the game that the option should be not available)

        bool enabled = true;
        std::vector<Yarn::Operand> substitutions;


        int substitutionsCt = (int)get_float_operand(instruction, 2);

        int nOperands = instruction.operands_size();

        if (nOperands < 2)
        {
            YARN_EXCEPTION("Insufficient arguments to ADD_OPTION");
        }

        if (nOperands > 2)
        {
            if (substitutionsCt)
            {
                substitutions.resize(substitutionsCt);

                for (int i = 0; i < substitutionsCt; i++)
                {
                    substitutions[i] = variableStack.top();
                    variableStack.pop();
                }
            }
        }

        if ((nOperands > 3) && (get_bool_operand(instruction, 3)))
        {
            Yarn::Operand& top = variableStack.top();

            if ((!top.has_bool_value()) || (top.has_float_value() && (top.float_value() == 0.f)))
            {
                YARN_EXCEPTION("ADD_OPTION: condition on top of stack not convertible to a bool value");
            }

            enabled = top.bool_value();
            variableStack.pop();
        }

        assert(substitutionsCt == substitutions.size());

        Option opt =
        {
            Line {get_string_operand(instruction,0), substitutions},
            get_string_operand(instruction,1),
            enabled
        };

        currentOptionsList.push_back(opt);
    }
    break;
    case Yarn::Instruction_OpCode_SHOW_OPTIONS:
    {
        assert(currentOptionsList.size());

        runningState = AWAITING_INPUT;

        callbacks.onShowOptions(currentOptionsList);
    }
    break;
    case Yarn::Instruction_OpCode_PUSH_STRING:
    {
        if (!instruction.operands_size() >= 1)
        {
            YARN_EXCEPTION("Missing operand for PUSH_STRING instruction");
        }

        const Yarn::Operand& op = instruction.operands(0);

        if (!op.has_string_value())
        {
            YARN_EXCEPTION("Passed non-string operand to PUSH_STRING instruction");
        }

        variableStack.push(op);
    }
    break;
    case Yarn::Instruction_OpCode_PUSH_FLOAT:
    {
        if (!instruction.operands_size() >= 1)
        {
            YARN_EXCEPTION("Missing operand for PUSH_FLOAT instruction");
        }

        const Yarn::Operand& op = instruction.operands(0);

        if (!op.has_float_value())
        {
            YARN_EXCEPTION("Passed non-float operand to PUSH_FLOAT instruction");
        }

        variableStack.push(op);
    }
    break;
    case Yarn::Instruction_OpCode_PUSH_BOOL:
    {
        if (!instruction.operands_size() >= 1)
        {
            YARN_EXCEPTION("Missing operand for PUSH_BOOL instruction");
        }

        const Yarn::Operand& op = instruction.operands(0);

        if (!op.has_bool_value())
        {
            YARN_EXCEPTION("Passed non-bool operand to PUSH_BOOL instruction");
        }

        variableStack.push(op);
    }
    break;
    case Yarn::Instruction_OpCode_PUSH_NULL:
    {
        Yarn::Operand op;
        op.clear_value();

        variableStack.push(op);
    }
    break;
    case Yarn::Instruction_OpCode_JUMP_IF_FALSE:
    {
        // Jumps to the named position in the the node, if the top of the
        // stack is not null, zero or false.
        // opA = string: label name 

        const Yarn::Operand& top = variableStack.top();

        if (!(top.has_string_value() || (top.has_bool_value() && (top.bool_value() != false)) || (top.has_float_value() && (top.float_value() != 0.f))))
        {
            const std::string& labelName = get_string_operand(instruction, 0);

            if (currentNode->labels().find(labelName) == currentNode->labels().end())
            {
                YARN_EXCEPTION("Missing jump label in JUMP_IF_FALSE instruction: " + labelName);
            }

            std::int32_t translatedLabel = currentNode->labels().at(labelName);

            setInstruction(translatedLabel);
        }
    }
    break;
    case Yarn::Instruction_OpCode_POP:
    {
        if (!variableStack.size())
        {
            YARN_EXCEPTION("POP instruction called with 0 stack size");
        }
        variableStack.pop();
    }
    break;
    case Yarn::Instruction_OpCode_CALL_FUNC:
    {
        int a = instruction.operands_size();

        const std::string& varname = get_string_operand(instruction, 0);

        assert(variableStack.size());

        auto operandCt = variableStack.top().float_value();

        variableStack.pop();

        if (this->functions.find(varname) == this->functions.end())
        {
            YARN_EXCEPTION("Missing function with identifier : " + varname);
        }

        auto rval = this->functions[varname](*this, operandCt);

        this->variableStack.push(rval);
    }
    break;
    case Yarn::Instruction_OpCode_PUSH_VARIABLE:
    {
        const std::string& varname = get_string_operand(instruction, 0);

        variableStack.push(variableStorage[varname]);
    }
    break;
    case Yarn::Instruction_OpCode_STORE_VARIABLE:
    {
        const std::string& varname = get_string_operand(instruction, 0);

        if (!variableStack.size())
        {
            YARN_EXCEPTION("STORE_VARIABLE instruction called with empty stack size");
        }

        variableStorage[varname] = variableStack.top();
    }
    break;
    case Yarn::Instruction_OpCode_STOP:
    {
        runningState = STOPPED;

        callbacks.onProgramStopped();

        return;
    }
    break;
    case Yarn::Instruction_OpCode_RUN_NODE:
    {
        if (variableStack.size() && variableStack.top().has_string_value())
        {
            const std::string& nodeName = variableStack.top().string_value();

            loadNode(nodeName);

            variableStack.pop();

            return;
        }
        else
        {
            YARN_EXCEPTION("RUN_NODE instruction expected string variable on top of stack");
        }

    }
    break;
    default:
    {
        YARN_EXCEPTION("Unknown instruction opcode");
        break;
    }
    };

    advance();
}
