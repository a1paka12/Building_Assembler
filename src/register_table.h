#ifndef REGISTER_TABLE_H
#define REGISTER_TABLE_H

// 배열의 인덱스가 ABI 레지스터 번호다. fp는 s0의 추가 별칭이다.
static const char *const abi_registers[] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

#endif
