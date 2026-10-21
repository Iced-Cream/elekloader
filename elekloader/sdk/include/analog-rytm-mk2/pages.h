/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-ar2 0.4: parameter pages and parameter overrides (docs/ADAPTING.md,
 * "Parameter pages (core-ar2 0.4, Analog Rytm MKII)"). Core owns the entries
 * of stock's four page routines (page_info, page_get_value,
 * param_apply_delta, param_value_text) and asks the mods through two tables,
 * contributed in mod.json:
 *
 *     "contribute": [
 *       {"to": "core_pages", "order": 14, "data": "00000000",
 *        "relocs": [[0, "abs32", "sym:my_page", 0]]},
 *       {"to": "core_params", "order": 50, "data": "00000000",
 *        "relocs": [[0, "abs32", "sym:my_params", 0]]}]
 *
 * and in C:
 *
 *     const struct core_page my_page = {14, "MY PAGE", {449, 450, 0, 0, 0, 0, 0, 0}};
 *     const struct core_param my_params = {449, 450, my_get, my_delta, my_text};
 *
 * Entries are asked in table order (by "order"); the first that answers wins.
 * A page id must be one stock does not have (stock's are 0..11) and no other
 * mod uses; the page key's list still has to name it (a keep2 on the list's
 * address, as in mods/core-ar2/README.md). A function left 0 passes.
 */
#ifndef ELEKLOADER_AR2_PAGES_H
#define ELEKLOADER_AR2_PAGES_H

#include <stdint.h>

/* page_info answers &page->name: the rest is stock's entry layout */
struct core_page {
    int32_t page;
    const char *name;
    int32_t ids[8];         /* knobs A..H, 0 for none */
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
