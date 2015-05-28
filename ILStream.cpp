#include "ILStream.h"

// If the first byte of the standard encoding is 0xFF, then
// the second byte can be used as 1 byte encoding.  Otherwise                                                               l   b         b
// the encoding is two bytes.                                                                                               e   y         y
//                                                                                                                          n   t         t
//                                                                                                                          g   e         e
//                                                                                                           (unused)       t
//  Canonical Name                    String Name              Stack Behaviour           Operand Params    Opcode Kind      h   1         2    Control Flow
// -------------------------------------------------------------------------------------------------------------------------------------------------------
/*
OPDEF(CEE_NOP,                        "nop",              Pop0,               Push0,       InlineNone,         IPrimitive,  1,  0xFF,    0x00,    NEXT)
*/

#define OPDEF(canonical_name, string_name, pre_stack_op, post_stack_op, paramtype, opcode_kind, opcode_length_, byte1, byte2, flow) \
{                             \
    Instruction i;            \
    i.name = (string_name);   \
    i.opcode1 = (byte1);        \
    i.opcode2 = (byte2);        \
    i.opcode_length = (opcode_length_); \
    i.param = (paramtype);    \
    opcodes.push_back(i);     \
}

std::vector<Instruction> CreateInstructionSet()
{
    std::vector<Instruction> opcodes;
    #include "OpCodes.h"
    return opcodes;
}
