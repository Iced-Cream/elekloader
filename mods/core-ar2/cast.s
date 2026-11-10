| SPDX-License-Identifier: GPL-2.0-or-later
| core-ar2 0.14: the UI skip, a memo on stock's __dynamic_cast (libstdc++,
| 0x401b82f2: (src, src_type, dst_type, src2dst) -> the cast pointer or 0).
|
| The UI asks "is view X open?" about twenty times per main-screen refresh
| (0x400c9272 and its finders 0x401a1ed6..0x401a2d02), each by casting every
| entry of the open-view list. The views use multiple inheritance, so each
| cast walks the class tree and compares type names with strcmp: about two
| thirds of the CPU the UI uses outside the audio render.
|
| A cast's answer depends only on the source subobject's vtable pointer (it
| fixes the complete type and where the subobject sits in it), the two types
| and the hint; the cast pointer is the source pointer plus a fixed offset.
| Vtables and type infos are in flash, so the memo's answer is always stock's.
| Construction and destruction vtables are other pointers, so they are other
| keys.
|
| The memo has 128 entries of 32 bytes, direct-mapped: the vtable pointer,
| the two types, the hint, the offset and a found flag. A miss runs stock's
| cast and keeps its answer. Lookup and store run with interrupts masked (a
| dozen instructions), so tasks that cast at the same time never see half an
| entry.
|
| On while core_ui_skip_want is nonzero and core_ui_skip_hold is 0
| (analog-rytm-mk2/ui.h); off, the site is stock's cast after two tests.
|
| Site: 0x401b82f2, stock `link.w %a6,#-40; movem.l %d2-%d5/%a2,(%sp)`
| (4e56ffd8 48d7043c), replaced by a jmp here.

        .equ    CAST_REST, 0x401b82fa
        .equ    MEMO_N, 128

        .text
        .globl  core_cast_site, core_cast_stock
        .globl  core_ui_skip_hold_add, core_ui_skip_hold_release
        .align  2
core_cast_site:
        tst.b   core_ui_skip_want
        beq.w   9f
        tst.l   core_ui_skip_hold
        bne.w   9f
        lea     -12(%sp),%sp
        movem.l %d2-%d3/%a2,(%sp)
        | the arguments: 16(%sp) src, 20 src_type, 24 dst_type, 28 src2dst
        movea.l 16(%sp),%a0
        move.l  (%a0),%d1               | the vtable pointer
        move.l  24(%sp),%d0             | dst_type
        move.l  %d1,%d2                 | the entry: mix the key's words
        eor.l   %d0,%d2
        add.l   28(%sp),%d2
        move.l  %d2,%d3
        lsr.l   #7,%d3
        eor.l   %d3,%d2
        lsr.l   #2,%d2
        andi.l  #MEMO_N-1,%d2
        lsl.l   #5,%d2
        lea     core_cast_memo,%a1
        adda.l  %d2,%a1
        move.w  %sr,%d3
        move.w  #0x2700,%sr
        cmp.l   (%a1),%d1
        bne.s   5f
        cmp.l   8(%a1),%d0
        bne.s   5f
        move.l  20(%sp),%d2
        cmp.l   4(%a1),%d2
        bne.s   5f
        move.l  28(%sp),%d2
        cmp.l   12(%a1),%d2
        bne.s   5f
        | a hit
        move.l  16(%a1),%d0             | the offset
        move.l  20(%a1),%d2             | found
        addq.l  #1,core_ui_skip_hits
        move.w  %d3,%sr
        tst.l   %d2
        beq.s   2f
        add.l   %a0,%d0
        bra.s   3f
2:      moveq   #0,%d0
3:      movem.l (%sp),%d2-%d3/%a2
        lea     12(%sp),%sp
        rts

        | a miss: stock's cast, then keep its answer
5:      addq.l  #1,core_ui_skip_misses
        move.w  %d3,%sr
        move.l  %d1,%d2                 | the vtable pointer
        movea.l %a1,%a2                 | the entry
        move.l  28(%sp),-(%sp)
        move.l  28(%sp),-(%sp)
        move.l  28(%sp),-(%sp)
        move.l  28(%sp),-(%sp)
        bsr.w   9f
        lea     16(%sp),%sp
        move.w  %sr,%d3
        move.w  #0x2700,%sr
        move.l  %d2,(%a2)
        move.l  20(%sp),%d1
        move.l  %d1,4(%a2)
        move.l  24(%sp),%d1
        move.l  %d1,8(%a2)
        move.l  28(%sp),%d1
        move.l  %d1,12(%a2)
        moveq   #0,%d1
        tst.l   %d0
        beq.s   6f
        moveq   #1,%d1
        move.l  %d0,%d2
        sub.l   16(%sp),%d2
        move.l  %d2,16(%a2)
        bra.s   7f
6:      clr.l   16(%a2)
7:      move.l  %d1,20(%a2)
        move.w  %d3,%sr
        movem.l (%sp),%d2-%d3/%a2
        lea     12(%sp),%sp
        rts

| stock's cast: the two instructions the site replaced, then the rest
core_cast_stock:
9:      link.w  %a6,#-40
        movem.l %d2-%d5/%a2,(%sp)
        jmp     CAST_REST

| a mod that needs stock's exact UI load (one that measures it) holds the
| skip off while it does; one instruction each, so any context may call them
core_ui_skip_hold_add:
        addq.l  #1,core_ui_skip_hold
        rts
core_ui_skip_hold_release:
        subq.l  #1,core_ui_skip_hold
        rts

        .bss
        .align  4
        .globl  core_ui_skip_want, core_ui_skip_hold
        .globl  core_ui_skip_hits, core_ui_skip_misses
core_cast_memo:
        .space  MEMO_N*32
core_ui_skip_hold:
        .space  4
core_ui_skip_hits:
        .space  4
core_ui_skip_misses:
        .space  4
core_ui_skip_want:
        .space  1
