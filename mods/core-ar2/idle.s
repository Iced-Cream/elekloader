| SPDX-License-Identifier: GPL-2.0-or-later
| core-ar2 0.12: the silent-voice skip's site (idle.c has the rules).
|
| 0x40121208, in the render's slot loop (0x40121152): `movea.l
| (0,%a5,%d4.l),%a1; tst.l %a1`, before stock's `beq.b 0x40121248`, which
| skips the slot's machine call when %a1 is 0. Live here: %d0 the slot's
| owner track, %d2 the slot, %d4 the slot x 4, %a2 the slot's parameter
| pointer (0x419ca0f0 + %d4), %a5 the machine pointers (0x419ca110), %d7 the
| tracks' flag bytes (0x8000ea18), %a4 the tracks' pitches (0x80006388).
| %d1, %d6 and %a0 are free (stock reloads them). The site returns %a1 and
| its flags: 0 takes stock's skip, the pointer runs the call.
|
| The busy path, a voice whose machine gain is above 2^-19 of full scale
| (|gain| >= 0x1000 in either of the slot's gain words, 0x80007000 + 8 x
| slot), is a dozen instructions: it forgets the slot's watch and returns
| the pointer.
        .equ    GAINS, 0x80007000

        .text
        .globl  core_voice_site
        .align  2
core_voice_site:
        movea.l (0,%a5,%d4.l),%a1
        tst.l   %a1
        beq.s   9f                      | no machine yet: stock's skip
        lea     GAINS,%a0
        move.l  (0,%a0,%d4.l*2),%d1     | the gain now
        addi.l  #0xfff,%d1
        cmpi.l  #0x1ffe,%d1
        bhi.s   8f                      | |gain| >= 0x1000: sounding
        move.l  (4,%a0,%d4.l*2),%d1     | the gain it ramps to
        addi.l  #0xfff,%d1
        cmpi.l  #0x1ffe,%d1
        bls.s   1f
8:      lea     core_voice_state,%a0
        clr.l   (0,%a0,%d4.l*2)         | watch again from the start
        clr.l   (4,%a0,%d4.l*2)
9:      tst.l   %a1
        rts
1:      move.l  %d0,-(%sp)              | the owner track, for the call after
        move.l  %a1,-(%sp)
        move.l  %d0,-(%sp)
        move.l  %d2,-(%sp)
        jsr     core_voice_quiet        | (slot, owner, pointer): 0 or pointer
        lea     12(%sp),%sp
        movea.l %d0,%a1
        move.l  (%sp)+,%d0
        tst.l   %a1
        rts
