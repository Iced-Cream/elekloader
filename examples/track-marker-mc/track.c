/* SPDX-License-Identifier: GPL-2.0-or-later */
/* track-marker for the Model:Cycles: the smallest mod that names core-mc 0.2's firmware locations
 * (docs/ADAPTING.md, "Firmware locations (core-mc 0.2, Model:Cycles)").
 *
 * After the firmware has drawn each frame, core calls track_draw(bmp, ctrl) (ev_draw), and this
 * puts a 3 x 3 square in the bottom row, under the active track: x = 4 + 20 t for track t, 0-5.
 * It names no address of its own: fw_fill and fw_active_track's walk (fw_project,
 * fw_project_tracks, fw_selection_active) are core's exports, resolved by the linker. */
#include "model-cycles/core.h"

void track_draw(void *bmp, void *ctrl)
{
    int32_t t = fw_active_track();

    (void)ctrl;
    if (t >= 0)
        fw_fill(bmp, 4 + 20 * t, 0, 6 + 20 * t, 2, 1, 1);
}
