| SPDX-License-Identifier: GPL-2.0-or-later
| core-ar2 0.9: the sites of the mod p-lock rows (rows.c). Each is a jmp
| site at a routine's entry: core runs the routine (its displaced
| instructions, then a jump back into it), then its own part (OS 1.74).
        .text
        .globl  core_memcpy, core_row_load, core_rebuild, core_trig_locks
        .align  2

        .equ    PATTERN_SIZE, 85760

| 0x40098bc0, memcpy (dst, src, n): `movea.l 4(%sp),%a1; movea.l 8(%sp),%a0`.
| A whole pattern object's copy also copies its mod rows.
core_memcpy:
        move.l  12(%sp),%d0
        cmp.l   #PATTERN_SIZE,%d0
        beq.s   1f
        movea.l 4(%sp),%a1
        movea.l 8(%sp),%a0
        jmp     0x40098bc8
1:      move.l  12(%sp),-(%sp)
        move.l  12(%sp),-(%sp)
        move.l  12(%sp),-(%sp)
        jsr     memcpy_body
        lea     12(%sp),%sp
        move.l  %d0,-(%sp)
        move.l  12(%sp),-(%sp)            | src
        move.l  12(%sp),-(%sp)            | dst
        jsr     core_rows_copy
        addq.l  #8,%sp
        move.l  (%sp)+,%d0
        rts
memcpy_body:
        movea.l 4(%sp),%a1
        movea.l 8(%sp),%a0
        jmp     0x40098bc8

| 0x40008f98, row load (live lock store, rows): `lea -36(%sp),%sp; movem.l
| %d2-%d6/%a2-%a5,(%sp)`. Then the rows with mod codes become the shadow.
core_row_load:
        move.l  8(%sp),-(%sp)
        move.l  8(%sp),-(%sp)
        jsr     row_load_body
        addq.l  #8,%sp
        move.l  %d0,-(%sp)
        move.l  12(%sp),-(%sp)            | rows
        move.l  12(%sp),-(%sp)            | the lock store
        jsr     core_rows_load
        addq.l  #8,%sp
        move.l  (%sp)+,%d0
        rts
row_load_body:
        lea     -36(%sp),%sp
        movem.l %d2-%d6/%a2-%a5,(%sp)
        jmp     0x40008fa0

| 0x40009216, rebuild (rows, live lock store, ...): `lea -48(%sp),%sp;
| movem.l %d2-%d7/%a2-%a6,(%sp)`. Then the shadow's rows go after stock's.
core_rebuild:
        move.l  12(%sp),-(%sp)
        move.l  12(%sp),-(%sp)
        move.l  12(%sp),-(%sp)
        jsr     rebuild_body
        lea     12(%sp),%sp
        move.l  %d0,-(%sp)
        move.l  12(%sp),-(%sp)            | the lock store
        move.l  12(%sp),-(%sp)            | rows
        jsr     core_rows_save
        addq.l  #8,%sp
        move.l  (%sp)+,%d0
        rts
rebuild_body:
        lea     -48(%sp),%sp
        movem.l %d2-%d7/%a2-%a6,(%sp)
        jmp     0x4000921e

| 0x4009a954, a trig's locks (track, out, the step's locks, fine bytes):
| `clr.w %d0; lea -32(%sp),%sp`. Then core notes the trig's mod locks.
core_trig_locks:
        move.l  16(%sp),-(%sp)
        move.l  16(%sp),-(%sp)
        move.l  16(%sp),-(%sp)
        move.l  16(%sp),-(%sp)
        jsr     trig_locks_body
        lea     16(%sp),%sp
        move.l  %d0,-(%sp)
        move.l  16(%sp),-(%sp)            | the step's locks
        move.l  12(%sp),-(%sp)            | track
        jsr     core_rows_trig
        addq.l  #8,%sp
        move.l  (%sp)+,%d0
        rts
trig_locks_body:
        clr.w   %d0
        lea     -32(%sp),%sp
        jmp     0x4009a95a
