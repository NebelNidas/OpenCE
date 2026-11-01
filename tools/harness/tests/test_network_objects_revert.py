"""The host tells co-op clients of a unit a checkpoint revert brings back to life under the same index."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, enum_with, function, mutated, read, run


def generated(fault=None):
    source = read('port/linux/game/network_objects.c')
    types = enum_with(read('source/objects/object_types.h'), '_object_type_biped') + '\n'
    types += enum_with(read('source/objects/objects.h'), '_object_dead_bit') + '\n'
    types += enum_with(source, '_object_change_create') + '\n'
    types += '#define MAXIMUM_ENTRIES_PER_MESSAGE %d\n' % constant(source, 'MAXIMUM_ENTRIES_PER_MESSAGE')
    start = source.index('static long objects_host_told[MAXIMUM_TRACKED_OBJECTS];')
    end = source.index('} objects_host_inventories[MAXIMUM_TRACKED_OBJECTS];')
    code = source[start:source.index(';', end) + 1] + '\n' + function(source, 'distributed_host_update_objects')
    if fault == 'revived':
        code = mutated(code, 'objects_host_told_dead[absolute_index] && !dead', 'FALSE')
    if fault == 'inventory':
        code = mutated(code, 'objects_host_inventories[absolute_index].weapons_checksum = 0;', '')
    return (('types.inc', types), ('under_test.inc', code))


@pytest.mark.parametrize('case', ['revert', 'corpse', 'item', 'reuse'])
def test_case(case):
    status, output = run(build('network_objects_revert', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('fault,case', [('revived', 'revert'), ('inventory', 'revert')])
def test_negative_control(fault, case):
    status, output = run(build('network_objects_revert', generated(fault)), case)
    assert status == CHECK_FAILED, output
