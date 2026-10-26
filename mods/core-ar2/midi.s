| SPDX-License-Identifier: GPL-2.0-or-later
| core-ar2 0.8: the sites of mod parameters on MIDI (midi.c), OS 1.74.
        .text
        .globl  core_nrpn_site, core_cc_site
        .align  2

| 0x400b2b26, a track's NRPN in (obj, nrpn, value, track, flag): `lea
| -28(%sp),%sp; movem.l %d2-%d6/%a2-%a3,(%sp)`. An NRPN of MSB 4 is core's;
| any other goes on to stock.
core_nrpn_site:
        move.l  16(%sp),-(%sp)
        move.l  16(%sp),-(%sp)
        move.l  16(%sp),-(%sp)
        jsr     core_nrpn_in
        lea     12(%sp),%sp
        tst.l   %d0
        beq.s   1f
        moveq   #1,%d0
        rts
1:      lea     -28(%sp),%sp
        movem.l %d2-%d6/%a2-%a3,(%sp)
        jmp     0x400b2b2e

| 0x400b0544, a track's CC in (obj, cc, value, track, flag): `lea
| -20(%sp),%sp; movem.l %d2-%d5/%a2,(%sp)`. A mod parameter's CC is core's.
core_cc_site:
        move.l  16(%sp),-(%sp)
        move.l  16(%sp),-(%sp)
        move.l  16(%sp),-(%sp)
        jsr     core_cc_in
        lea     12(%sp),%sp
        tst.l   %d0
        beq.s   1f
        moveq   #1,%d0
        rts
1:      lea     -20(%sp),%sp
        movem.l %d2-%d5/%a2,(%sp)
        jmp     0x400b054c
