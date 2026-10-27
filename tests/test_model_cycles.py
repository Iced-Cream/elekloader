# SPDX-License-Identifier: GPL-2.0-or-later
"""The Model:Cycles: core-mc 0.2's firmware locations (pytest, or run with python).

Needs a stock file, which never goes in the repo, named by an environment
variable; a test whose input is missing is skipped, not passed:
  ELEKLOADER_MC_SYX   Model_Cycles_OS1.13.syx
Building the core and the example also needs the cross toolchain
(ELEKLOADER_CROSS, as for the other cores).

core-mc 0.2 is 0.1 and the firmware locations as exports (mods/core-mc/fw.s):
the same sites with the same bytes, the same tables, every symbol where it
was, the same code, and absolute symbols only.
"""
import json
import os
import re
import shutil
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, ROOT)

from elekloader import devices, elemod, formats, lint, link   # noqa: E402
from elekloader.sdk import build                               # noqa: E402

SYX = os.environ.get('ELEKLOADER_MC_SYX', '')
DEV = [d for d in devices.devices() if d.key == 'model-cycles'][0]
CORE_MC = os.path.join(ROOT, 'mods', 'core-mc')
FW_S = os.path.join(CORE_MC, 'fw.s')
CORE_H = os.path.join(ROOT, 'elekloader', 'sdk', 'include', 'model-cycles', 'core.h')
EXAMPLE = os.path.join(ROOT, 'examples', 'track-marker-mc')
# The exports that are routines: the OS's code calls each one (the others are data it names).
ROUTINES = {'fw_project', 'fw_project_tracks', 'fw_selection_active', 'fw_ui_states', 'fw_value_shown',
            'fw_trig_held', 'fw_key_held', 'fw_fill', 'fw_text', 'fw_font_make', 'fw_font_drop',
            'fw_view_redraw', 'fw_fs_lock', 'fw_fs_unlock', 'fw_fs_entry', 'fw_fs_read', 'fw_fs_find_id'}


class Skip(Exception):
    pass


_c = {}


def stock():
    if 'img' not in _c:
        if not SYX or not os.path.exists(SYX):
            raise Skip('missing %s' % (SYX or 'ELEKLOADER_MC_SYX'))
        st, dev, _rel = formats.load(SYX)
        assert dev is DEV
        _c['img'] = formats.main_image(st, DEV)
    return _c['img']


def toolchain():
    if not shutil.which(os.environ.get('ELEKLOADER_CROSS', DEV.toolchain['prefix']) + 'as'):
        raise Skip('no m68k cross assembler')


def fw_lines():
    """-> [(name, defsym name)] of fw.s's FW lines."""
    with open(FW_S) as fh:
        return re.findall(r'^\s+FW\s+(fw_\w+),\s+(FW_\w+)', fh.read(), re.M)


def built(mod_dir, tag):
    if tag not in _c:
        stock()
        toolchain()
        path, _m = build.build(mod_dir, SYX, tempfile.mkdtemp())
        with open(path) as fh:
            _c[tag] = (path, json.load(fh))
    return _c[tag]


def core_01_dir():
    """mods/core-mc as 0.1 had it: core.s alone, no FW_* values."""
    with open(os.path.join(CORE_MC, 'mod.json')) as fh:
        m = json.load(fh)
    m['version'] = '0.1'
    m['sources'] = [os.path.join(ROOT, 'mods', 'core', 'core.s')]
    m['defsym'] = {k: v for k, v in m['defsym'].items() if not k.startswith('FW_')}
    d = tempfile.mkdtemp()
    with open(os.path.join(d, 'mod.json'), 'w') as fh:
        json.dump(m, fh)
    return d


def test_the_names_agree():
    """fw.s exports what mod.json gives a value for, its comment table gives the same values, and
    core.h declares exactly those names (with its two inline helpers)."""
    with open(os.path.join(CORE_MC, 'mod.json')) as fh:
        defsym = json.load(fh)['defsym']
    pairs = fw_lines()
    assert pairs and all(d in defsym for _n, d in pairs)
    assert {d for d in defsym if d.startswith('FW_')} == {d for _n, d in pairs}
    with open(FW_S) as fh:
        table = dict(re.findall(r'^\|\s+(fw_\w+)\s+(0x[0-9a-f]+)', fh.read(), re.M))
    assert {n: int(table[n], 16) for n, _d in pairs} == {n: int(defsym[d], 16) for n, d in pairs}
    with open(CORE_H) as fh:
        names = set(re.findall(r'\b(fw_\w+)\b', fh.read())) - {'fw_active_track', 'fw_track_sound'}
    assert names == {n for n, _d in pairs}


def test_each_export_is_a_routine_or_data_the_os_names():
    """A routine export follows an rts or is the target of an absolute jsr; a data export is an
    operand of the OS's code."""
    img = stock()
    with open(os.path.join(CORE_MC, 'mod.json')) as fh:
        defsym = json.load(fh)['defsym']
    for name, d in fw_lines():
        addr = int(defsym[d], 16)
        word = struct.pack('>I', addr)
        if name in ROUTINES:
            off = addr - DEV.main_load
            assert addr % 2 == 0 and 2 <= off < len(img), name
            assert img[off - 2:off] == b'\x4e\x75' or img.count(b'\x4e\xb9' + word), name
        else:
            assert img.count(word), name


def test_core_mc_0_2_is_0_1_and_more():
    """The same sites, tables, symbols, relocations and code as 0.1; only absolute exports added."""
    _p, new = built(CORE_MC, 'new')
    _p, old = built(core_01_dir(), 'old')
    assert old['version'] == '0.1' and new['version'] == '0.2'
    assert new['sites'] == old['sites'] and new['collections'] == old['collections']
    assert set(old['exports']) <= set(new['exports'])
    assert all(new['symbols'][k] == v for k, v in old['symbols'].items())
    added = set(new['exports']) - set(old['exports'])
    assert added == {n for n, _d in fw_lines()}
    assert all(new['symbols'][n][0] == 'abs' for n in added)
    assert new['relocs'] == old['relocs']
    img = stock()
    m_old, m_new = link.Mod2(old, 'old'), link.Mod2(new, 'new')
    for s in ('.boot', '.run'):
        assert elemod.parts_bytes(m_new.sections[s]['parts'], img, DEV) == \
            elemod.parts_bytes(m_old.sections[s]['parts'], img, DEV), s


def test_the_example_needs_core_0_2():
    """examples/track-marker-mc names only core's exports: it links with 0.2, and 0.1 is refused."""
    new, _m = built(CORE_MC, 'new')
    old, _m = built(core_01_dir(), 'old')
    ex, m = built(EXAMPLE, 'example')
    assert not m['sites'] and set(m['imports']) <= {n for n, _d in fw_lines()}
    assert lint.main([ex, '--stock', SYX, '--with', new]) == 0
    assert lint.main([ex, '--stock', SYX, '--with', old]) != 0


if __name__ == '__main__':
    import traceback
    ok = skipped = failed = 0
    for n in sorted(k for k in globals() if k.startswith('test_')):
        try:
            globals()[n]()
            print('ok      %s' % n)
            ok += 1
        except Skip as e:
            print('SKIP    %s (%s)' % (n, e))
            skipped += 1
        except Exception:
            print('FAIL    %s' % n)
            traceback.print_exc()
            failed += 1
    print('%d passed, %d failed, %d skipped' % (ok, failed, skipped))
    sys.exit(1 if failed else 0)
