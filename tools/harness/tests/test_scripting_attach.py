"""objects_scripting_attach from the sources: a weapon put away in an inventory is not attached."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, mutated, read, run


def generated(fault=None):
    attach = function(read('source/objects/objects.c'), 'objects_scripting_attach')
    if fault == 'stowed-attached':
        attach = mutated(attach, ' &&\n\t\tTEST_FLAG(object_get(child_object_index)->object.flags, _object_connected_to_map_bit)', '')
    flags = enum_with(read('source/objects/objects.h'), '_object_connected_to_map_bit')
    return (('flags.inc', flags), ('under_test.inc', attach))


@pytest.mark.parametrize('case', ['loose', 'stowed', 'held'])
def test_scripting_attach(case):
    status, output = run(build('scripting_attach', generated()), case)
    assert status == 0, output


def test_stowed_attached_fails():
    status, output = run(build('scripting_attach', generated('stowed-attached')), 'stowed')
    assert status == CHECK_FAILED, output
