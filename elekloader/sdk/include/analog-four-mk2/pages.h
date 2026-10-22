/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-a4 0.2: parameter pages and parameter overrides (docs/ADAPTING.md,
 * "Parameter pages (core-a4 0.2, Analog Four MKII)"): core-ar2 0.4's
 * (analog-rytm-mk2/pages.h) at the A4's routines. Core owns the entries of
 * stock's four page routines (page_info, page_get_value, param_apply_delta,
 * param_value_text) and asks the mods through two tables, contributed in
 * mod.json:
 *
 *     "contribute": [
 *       {"to": "core_pages", "order": 50, "data": "00000000",
 *        "relocs": [[0, "abs32", "sym:my_page", 0]]},
 *       {"to": "core_params", "order": 50, "data": "00000000",
 *        "relocs": [[0, "abs32", "sym:my_params", 0]]}]
 *
 * and in C:
 *
 *     const struct core_page my_page = {50, "MY PAGE", {449, 450}};
 *     const struct core_param my_params = {449, 450, my_get, my_delta, my_text};
 *
 * Entries are asked in table order (by "order"); the first that answers wins.
 * A page id must be one stock does not have (stock's are 0..47) and no other
 * mod uses. A function left 0 passes.
 *
 * What differs from the Rytm: a page has ten knobs (A..J), not eight, and
 * there is no core_view_pages. Each A4 page is a view of its own, and core-a4
 * 0.2 adds no views, so core_pages answers page_info for a page a mod's own
 * view shows; core_params works on any page, stock's included.
 */
#ifndef ELEKLOADER_A4_PAGES_H
#define ELEKLOADER_A4_PAGES_H

#include <stdint.h>

/* page_info answers &page->name: the rest is stock's entry layout (0x2c bytes) */
struct core_page {
    int32_t page;
    const char *name;
    int32_t ids[10];        /* knobs A..J, 0 for none */
};

/* param_value_text's own arguments */
struct core_text {
    void *obj;
    int32_t id;
    int32_t value;          /* 8.8 */
    char *buf;
};

#define CORE_TEXT_PASS 0    /* not this entry's: the next entry, then stock */
#define CORE_TEXT_DONE 1    /* buf is written */
#define CORE_TEXT_STOCK 2   /* stock formats t->id and t->value (changed or not) */

struct core_param {
    int32_t first, last;    /* the ids this entry answers for */
    /* 1 and *value (8.8) when it answers, 0 to pass */
    int (*get)(int32_t id, int32_t *value);
    /* delta in 8.8, func 1 while FUNC is held; 1 when it took the turn (core
     * redraws the view and stock makes no lock), 0 to pass */
    int (*delta)(void *view, int32_t id, int32_t delta, int32_t func);
    int (*text)(struct core_text *t);
};

#endif
