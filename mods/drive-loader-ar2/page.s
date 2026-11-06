| SPDX-License-Identifier: GPL-2.0-or-later
| drive-loader (Analog Rytm MKII, OS 1.74): the MODS page's label callbacks
| (page.c). Stock's MenuItem calls a label as a std::function returning a
| std::string: the string to build is at %a0 (the hidden result), the
| functor's address is the first stack argument, and %d0 returns %a0. The
| text comes from page.c; std::string(%a0, text, allocator) is stock's
| 0x401bcff0, as the Rytm's own labels (MIDI CONFIG's at 0x400da074) do.

        .text
        .align  2
        .globl  dl_mods_label, dl_row_label

| SETTINGS > GLOBAL's MODS row
dl_mods_label:
        link    %a6, #-4
        move.l  %d2, -(%sp)
        move.l  %a0, %d2
        pea     -1(%a6)                 | the allocator, a byte on the frame
        pea     dl_mods_name
        move.l  %d2, -(%sp)
        jsr     0x401bcff0
        lea     12(%sp), %sp
        move.l  %d2, %d0
        movea.l %d2, %a0
        move.l  -8(%a6), %d2
        unlk    %a6
        rts

| a MODS page row: the functor's first word is the row's index
dl_row_label:
        link    %a6, #-4
        move.l  %d2, -(%sp)
        move.l  %a0, %d2
        movea.l 8(%a6), %a1             | the functor
        move.l  (%a1), -(%sp)
        jsr     dl_row_text             | -> the text, in %d0
        addq.l  #4, %sp
        pea     -1(%a6)
        move.l  %d0, -(%sp)
        move.l  %d2, -(%sp)
        jsr     0x401bcff0
        lea     12(%sp), %sp
        move.l  %d2, %d0
        movea.l %d2, %a0
        move.l  -8(%a6), %d2
        unlk    %a6
        rts

dl_mods_name:
        .asciz  "MODS"
