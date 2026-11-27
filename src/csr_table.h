#ifndef CSR_TABLE_H
#define CSR_TABLE_H

static const struct {
    const char *name;
    int number;
} csr_names[] = {
    {"mstatus",  0x300},
    {"mie",      0x304},
    {"mtvec",    0x305},
    {"mscratch", 0x340},
    {"mepc",     0x341},
    {"mcause",   0x342},
    {"mtval",    0x343},
    {"mip",      0x344},
    {"mhartid",  0xf14}
};

#endif
