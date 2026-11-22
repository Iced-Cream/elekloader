| SPDX-License-Identifier: GPL-2.0-or-later
| payload-check (Analog Rytm MKII): a 225 KB .payload of a known pattern,
| sidecar-check's (examples/sidecar-check-ar2/payload.s) with 900 blocks
| of 64 words. Block i is the word i ^ 0x5041594c ('PAYL'), the next 48
| words of one xorshift32 stream seeded with 0x5041594c (it runs on across
| blocks), then the 15 words of check.c's table. Nothing refers to it: a
| +Drive mod's payload stays on the +Drive, read through dl_payload_read.

        .equ    BLOCKS, 800                     | 204,800 bytes (200 KB): the update over the first
        .equ    RANDOM, 48

        .macro  table
        .long   0xe15d9d01
        .long   0x8c73fe28
        .long   0x8e85bde5
        .long   0x459dd5a5
        .long   0x4bda3694
        .long   0x4a421a4e
        .long   0x32edf4c4
        .long   0xf2fd80c7
        .long   0xfad6503f
        .long   0x0ea70bdd
        .long   0x22906a7a
        .long   0xe152535d
        .long   0x8144ee59
        .long   0xc5b3986e
        .long   0x15f6f9c3
        .endm

        .section .payload, "a"
        .align  4
pck_payload:
        .set    i, 0
        .set    x, 0x5041594c
        .rept   BLOCKS
        .long   i ^ 0x5041594c
        .rept   RANDOM
        .set    x, (x ^ (x << 13)) & 0xffffffff
        .set    x, x ^ (x >> 17)
        .set    x, (x ^ (x << 5)) & 0xffffffff
        .long   x
        .endr
        table
        .set    i, i + 1
        .endr
pck_payload_end:
