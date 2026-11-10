| SPDX-License-Identifier: GPL-2.0-or-later
| smpl-machines: the render's call of the sample layer's trig intake
| (0x40121134, `jsr 0x4011c1e0`, OS 1.74). The render ORs the voices whose
| sample layer starts this block into %d3 and those that stop into %d5
| (0x401210ee, 0x401210f8), pushes both and calls 0x4011c1e0, which takes
| voice 0 and hands the rest to 0x4011c6b8 through 0x8000586c; the render
| pushes %d5 and %d3 again for 0x4011c6b8 (0x4012127e), which stops voices
| 1..7 by %d5. Nothing else reads %d3 or %d5 between the two calls, and
| 0x4011c1e0 keeps them (C). So smpl_gate changes both the registers and
| the pushed copies: a voice whose track runs a SMPL machine starts nothing
| and stops what it played, in both routines.
        .text
        .globl  smpl_gate_s
        .align  2
smpl_gate_s:
        move.l  %d5,-(%sp)              | masks[1]: stops
        move.l  %d3,-(%sp)              | masks[0]: starts
        pea     (%sp)
        jsr     smpl_gate
        addq.l  #4,%sp
        move.l  (%sp)+,%d3
        move.l  (%sp)+,%d5
        move.l  %d3,4(%sp)              | the pushed copies, past the return address
        move.l  %d5,8(%sp)
        jmp     SAMPLE_INTAKE
