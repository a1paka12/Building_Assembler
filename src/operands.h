#ifndef OPERANDS_H
#define OPERANDS_H

struct IformatOperands {
    int rd;
    int rs1;
    int imm;
};

struct RformatOperands {
    int rd;
    int rs1;
    int rs2;
};

struct SBformatOperands {
    int rs1;
    int rs2;
    int imm;
};

struct UJformatOperands {
    int rd;
    int imm;
};

#endif
