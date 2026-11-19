.text
_start:
    addi t0, zero, 7
    addi t1, x0, -2
    add  t2, t0, t1
    beq  t2, t2, done
    addi t2, x0, 99
done:
    jal  x0, done

.data
value: .word 0x12345678
