# SPDX-License-Identifier: GPL-2.0-or-later
"""Mod storage's lockfile and allocator, elekloader/store.py (pytest, or run
with python). Needs no stock file: js/test/store.test.ts checks the
TypeScript port against the same module's results."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

from elekloader import store                 # noqa: E402
from elekloader.elemod import ModError       # noqa: E402

DEV = 'analog-rytm-mk2'


def ids(start=0x10000000):
    n = [start]

    def nxt():
        n[0] += 1
        return n[0]
    return nxt


def where(lock):
    return {f['sym']: (f['scope'], f['offset'], f['bits']) for f in lock['fields']}


def refused(fn, *args):
    try:
        fn(*args)
    except ModError as e:
        return str(e)
    raise AssertionError('not refused')


def test_a_new_lockfile_packs_from_bit_0():
    lock, _ = store.allocate(None, [('a', 'x', 'track', 4), ('a', 'y', 'track', 3),
                                    ('b', 'z', 'sound', 24)], DEV, ids())
    assert where(lock) == {'x': ('track', 0, 4), 'y': ('track', 4, 3), 'z': ('sound', 0, 24)}
    assert lock['ancestors'] == [] and lock['id'] == '0x10000001'
    assert lock['next']['track'] == 7 and lock['split'] == {'track': 2, 'pattern': 4}


def test_an_unchanged_build_keeps_the_layout_and_its_id():
    decls = [('a', 'x', 'track', 4), ('b', 'z', 'kit', 9)]
    l1, _ = store.allocate(None, decls, DEV, ids())
    l2, notes = store.allocate(l1, list(reversed(decls)), DEV, ids(0x20000000))
    assert l2 == l1 and notes == []


def test_fields_keep_their_place_and_new_ones_take_never_used_bits():
    l1, _ = store.allocate(None, [('a', 'x', 'pattern', 8), ('a', 'y', 'pattern', 8)], DEV, ids())
    l2, _ = store.allocate(l1, [('a', 'y', 'pattern', 8), ('b', 'w', 'pattern', 5)], DEV, ids(0x20000000))
    # x is freed; w does not reuse its bits, so w reads zero in every old project
    assert where(l2) == {'y': ('pattern', 8, 8), 'w': ('pattern', 16, 5)}
    assert [f['sym'] for f in l2['freed']] == ['x']
    assert l2['id'] == '0x20000001' and l2['ancestors'] == [l1['id']]
    assert l2['generation'] == l1['generation']


def test_a_changed_width_is_a_new_field():
    l1, _ = store.allocate(None, [('a', 'x', 'global', 8)], DEV, ids())
    l2, notes = store.allocate(l1, [('a', 'x', 'global', 9)], DEV, ids(0x20000000))
    assert where(l2) == {'x': ('global', 8, 9)}
    assert any('changed size' in n for n in notes)


def test_a_full_scope_is_repacked_under_a_new_generation():
    l1, _ = store.allocate(None, [('a', 'x', 'kit', 32), ('a', 'y', 'kit', 32)], DEV, ids())
    l2, _ = store.allocate(l1, [('a', 'y', 'kit', 32), ('b', 'z', 'kit', 16)], DEV, ids(0x20000000))
    assert l2['generation']['kit'] == l1['generation']['kit'] + 1
    assert where(l2) == {'y': ('kit', 0, 32), 'z': ('kit', 32, 16)}
    assert l2['freed'] == [] and l2['next']['kit'] == 48
    assert all(l2['generation'][s] == l1['generation'][s] for s in store.SCOPES if s != 'kit')


def test_the_split_grows_and_moves_the_store():
    l1, _ = store.allocate(None, [('a', 'x', 'pattern', 32), ('a', 'p', 'project', 8)], DEV, ids())
    l2, _ = store.allocate(l1, [('a', 'x', 'pattern', 32), ('a', 'y', 'pattern', 8),
                                ('a', 'p', 'project', 8)], DEV, ids(0x20000000))
    assert l2['split'] == {'track': 2, 'pattern': 5}
    for s in ('track', 'pattern', 'project'):
        assert l2['generation'][s] == l1['generation'][s] + 1
    for s in ('sound', 'kit', 'global'):
        assert l2['generation'][s] == l1['generation'][s]


def test_what_does_not_fit_is_refused():
    assert 'sound' in refused(store.allocate, None, [('a', 'x', 'sound', 25)], DEV, ids())
    # three bytes a pattern track would not fit in the store
    assert refused(store.allocate, None, [('a', 'x', 'track', 17)], DEV, ids())
    assert 'two mods' in refused(store.allocate, None, [('a', 'x', 'kit', 1), ('b', 'x', 'kit', 1)],
                                 DEV, ids())


def test_ancestors_keep_the_newest_15():
    lock, _ = store.allocate(None, [], DEV, ids())
    nxt = ids(0x20000000)
    for i in range(20):
        lock, _ = store.allocate(lock, [('a', 'f%d' % i, 'project', 1)], DEV, nxt)
    assert len(lock['ancestors']) == 15
    assert lock['ancestors'][0] == '0x20000013' and lock['id'] == '0x20000014'


def test_symbols_and_the_id_table():
    lock, _ = store.allocate(None, [('a', 'x', 'track', 4), ('a', 'y', 'project', 7)], DEV, ids())
    lock, _ = store.allocate(lock, [('a', 'x', 'track', 4), ('a', 'y', 'project', 7),
                                    ('a', 'z', 'sound', 3)], DEV, ids(0x20000000))
    syms, tab = store.symbols(lock, DEV)
    assert syms['x'] == 4 << 28 | 4 << 22 | 0
    assert syms['y'] == 6 << 28 | 7 << 22 | 0
    assert syms['z'] == 1 << 28 | 3 << 22 | 0
    assert syms['__store_gen'] == 0x01010101 and syms['__store_gen2'] == 0x01010204
    assert tab == [bytes.fromhex('20000001'), bytes.fromhex('10000001')]


def test_a_mods_storage_declarations():
    assert store.parse_decls({'storage': [{'sym': 'a_b', 'scope': 'kit', 'bits': 32}]}) == [('a_b', 'kit', 32)]
    for bad in ([{'sym': '1x', 'scope': 'kit', 'bits': 1}], [{'sym': 'x', 'scope': 'song', 'bits': 1}],
                [{'sym': 'x', 'scope': 'kit', 'bits': 33}], [{'sym': 'x', 'scope': 'kit', 'bits': 0}],
                [{'sym': 'x', 'scope': 'kit', 'bits': 1, 'more': 1}],
                [{'sym': 'x', 'scope': 'kit', 'bits': 1}, {'sym': 'x', 'scope': 'kit', 'bits': 1}], {}):
        refused(store.parse_decls, {'storage': bad})


def test_lockfiles_it_refuses():
    good, _ = store.allocate(None, [], DEV, ids())
    for k, v in (('elekloader_store', 2), ('device', 'digitakt'), ('id', '0x0'), ('fields', {})):
        bad = dict(good, **{k: v})
        refused(store.allocate, bad, [], DEV, ids())



def test_the_build_carries_the_lockfile():
    lock, _ = store.allocate(None, [('a', 'x', 'track', 4)], DEV, ids())
    image = bytes(1000) + b'any image' + store.trailer(lock)
    assert store.from_image(image) == lock
    assert store.from_image(bytes(1000)) is None
    assert 'cannot be read' in refused(store.from_image, store.TRAILER + (5).to_bytes(4, 'big') + b'{nope')


if __name__ == '__main__':
    for name, fn in sorted(globals().items()):
        if name.startswith('test_'):
            fn()
            print('ok', name)
