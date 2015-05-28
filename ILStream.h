#include <cstdint>
#include <string>
#include <vector>

enum ParamKind
{
     InlineNone = 0,

     ShortInlineVar = 1,
     ShortInlineI = 1,
     ShortInlineR = 1,
     ShortInlineBrTarget = 1,

     InlineI = 4,
     InlineI8 = 4,
     InlineField = 4,
     InlineString = 4,
     InlineVar = 4,
     InlineR = 4,
     InlineType = 4,
     InlineTok = 4,
     InlineMethod = 4,
     InlineBrTarget = 4,
     InlineSig = 4,
     InlineSwitch= 4,
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
