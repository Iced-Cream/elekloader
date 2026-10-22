| SPDX-License-Identifier: GPL-2.0-or-later
| core-a4 0.2: parameter pages and parameter overrides for mods, core-ar2
| 0.4's (mods/core-ar2/pages.s) at the Analog Four MKII's routines (OS 1.56).
|
| A parameter page goes through four stock routines: page_info(page), its
| entry {name, ids A..J}; page_get_value(view, id, lock), a knob's value in
| 8.8; param_apply_delta(view, id, delta, func), a turn; and
| param_value_text(obj, id, value, buf), the value's text. Core takes the
| first instructions of all four and hands them on through two tables:
|
|   core_pages   a page of the mod's own: {page, name, ids A..J}. page_info
|                answers its entry for that page id (stock's are 0..47).
|   core_params  {first id, last id, get, delta, text}: answers for the ids
|                in the range, the mod's own pages' knobs or stock ones.
|
| C convention throughout (analog-four-mk2/pages.h), as on the Rytm:
|   int get(int32_t id, int32_t *value)
|       1 and *value (8.8) when it answers, 0 to pass
|   int delta(void *view, int32_t id, int32_t delta, int32_t func)
|       delta in 8.8, func 1 while FUNC is held; 1 when it took the turn (core
|       then redraws the view, and stock makes no lock), 0 to pass
|   int text(struct core_text *t)
|       t is the routine's own arguments {obj, id, value, buf}: 0 to pass, 1
|       when it wrote buf, 2 when stock should go on with t's id and value,
|       which it may have changed (a page knob shown as a stock one)
| Entries are asked in the table's order; the first answer wins.
|
| The A4 has no counterpart of core-ar2's page-key lists (core_view_pages):
| each of its pages is a view of its own, and the page keys step through
| lists of views. 0.2 does not add views.
|
| Each stub is a jmp site's displaced instructions, reimplemented; the resume
| addresses come from mod.json's defsym (OS 1.56).
        .text
        .globl  core_page_info, core_param_get, core_param_delta, core_param_text
        .align  2

        .equ    P_FIRST, 0
        .equ    P_LAST, 4
        .equ    P_GET, 8
        .equ    P_DELTA, 12
        .equ    P_TEXT, 16

| page_info (0x400f6212): `moveq #47,%d1; movel %sp@(4),%d0`. A page in
| core_pages answers its entry, after the page id.
core_page_info:
        move.l  4(%sp),%d0
        lea     core_pages,%a1
1:      move.l  (%a1)+,%d1
        beq.s   9f
        move.l  %d1,%a0
        cmp.l   (%a0),%d0
        bne.s   1b
        addq.l  #4,%d1
        move.l  %d1,%d0
        rts
9:      moveq   #47,%d1
        jmp     PAGE_INFO_RET

| param_next(d0 = id, a1 = cursor in core_params) -> a0 = the next entry whose
| range holds the id, a1 past it; Z set when there is none. Clobbers d1.
param_next:
1:      move.l  (%a1)+,%d1
        beq.s   9f
        move.l  %d1,%a0
        cmp.l   P_FIRST(%a0),%d0
        blt.s   1b
        cmp.l   P_LAST(%a0),%d0
        bgt.s   1b
        moveq   #1,%d1
9:      rts

| page_get_value (0x40023d54): `lea %sp@(-12),%sp; moveml %d2-%d3/%a2,%sp@`.
| Frame: 0 the cursor, 4 the answer, 12 view, 16 id, 20 lock.
core_param_get:
        lea     core_params,%a1
        subq.l  #8,%sp
1:      move.l  16(%sp),%d0
        bsr.w   param_next
        beq.s   9f
        move.l  P_GET(%a0),%d1
        beq.s   1b
        move.l  %a1,(%sp)
        pea     4(%sp)
        move.l  %d0,-(%sp)
        move.l  %d1,%a0
        jsr     (%a0)
        addq.l  #8,%sp
        move.l  (%sp),%a1
        tst.l   %d0
        beq.s   1b
        move.l  4(%sp),%d0
        addq.l  #8,%sp
        rts
9:      addq.l  #8,%sp
        lea     -12(%sp),%sp
        movem.l %d2-%d3/%a2,(%sp)
        jmp     GET_RET

| param_apply_delta (0x400232e2): `lea %sp@(-12),%sp; moveml %d2-%d3/%a2,%sp@`.
| Frame: 0 the cursor, 8 view, 12 id, 16 delta, 20 func (its low byte).
core_param_delta:
        lea     core_params,%a1
        subq.l  #4,%sp
1:      move.l  12(%sp),%d0
        bsr.w   param_next
        beq.s   9f
        move.l  P_DELTA(%a0),%d1
        beq.s   1b
        move.l  %a1,(%sp)
        mvz.b   23(%sp),%d0
        move.l  %d0,-(%sp)                | func
        move.l  20(%sp),-(%sp)            | delta
        move.l  20(%sp),-(%sp)            | id
        move.l  20(%sp),-(%sp)            | view
        move.l  %d1,%a0
        jsr     (%a0)
        lea     16(%sp),%sp
        move.l  (%sp),%a1
        tst.l   %d0
        beq.s   1b
        move.l  8(%sp),-(%sp)             | taken: redraw the view now
        jsr     VIEW_INVALIDATE
        addq.l  #8,%sp
        rts
9:      addq.l  #4,%sp
        lea     -12(%sp),%sp
        movem.l %d2-%d3/%a2,(%sp)
        jmp     DELTA_RET

| param_value_text (0x4009ea68): `movel %d3,%sp@-; movel %d2,%sp@-;
| movel %sp@(20),%d2` (the Rytm's opens with lea and moveml). Frame: 0 the
| cursor, 8 obj, 12 id, 16 value, 20 buf; the handler gets the address of
| obj, the four as a struct core_text.
core_param_text:
        lea     core_params,%a1
        subq.l  #4,%sp
1:      move.l  12(%sp),%d0
        bsr.w   param_next
        beq.s   9f
        move.l  P_TEXT(%a0),%d1
        beq.s   1b
        move.l  %a1,(%sp)
        pea     8(%sp)
        move.l  %d1,%a0
        jsr     (%a0)
        addq.l  #4,%sp
        move.l  (%sp),%a1
        subq.l  #1,%d0
        bmi.s   1b                        | 0: not its id
        bne.s   9f                        | 2: stock goes on with t's id and value
        addq.l  #4,%sp                    | 1: the text is written
        rts
9:      addq.l  #4,%sp
        move.l  %d3,-(%sp)
        move.l  %d2,-(%sp)
        move.l  20(%sp),%d2
        jmp     TEXT_RET

