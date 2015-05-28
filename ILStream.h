#include <cstdint>
#include <string>
#include <vector>

enum ParamKind
{
     InlineBrTarget = 4,
     InlineField = 4,
     InlineI = 4,
     InlineI8 = 8,
     InlineMethod = 4,
     InlineNone = 0,
     InlineR = 8,
     InlineSig = 4,
     InlineSwitch = 4, /* TODO */
     InlineString = 4,
     InlineTok = 4,
     InlineType = 4,
     InlineVar = 2,

     ShortInlineBrTarget = 1,
     ShortInlineI = 1,
     ShortInlineR = 4,
     ShortInlineVar = 1,

};

struct Instruction
{
    std::string name;
    uint8_t opcode1;
    uint8_t opcode2;
    uint8_t opcode_length;
    ParamKind param;
};

std::vector<Instruction> CreateInstructionSet();
