"""The object name list from the sources: a name holds no object that was freed."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, enum_with, function, mutated, read, run


def generated(fault=None):
    objects = read('source/objects/objects.c')
    flags = enum_with(read('source/objects/objects.h'), '_object_garbage_bit') + '\n'
    flags += '#define MAXIMUM_OBJECT_NAMES_PER_SCENARIO %d\n' % constant(
        read('source/scenario/scenario.h'), 'MAXIMUM_OBJECT_NAMES_PER_SCENARIO')
    delete = function(objects, 'object_delete_recursive')
    if fault == 'not-forgotten':
        delete = mutated(delete, '\tobject_name_list_forget(object_index);\n', '')
    code = '\n'.join(function(objects, name) for name in (
        'object_name_list_forget', 'object_name_list_new', 'object_name_list_delete',
        'object_set_object_index_for_name_index', 'object_index_from_name_index')) + '\n' + delete
    return (('flags.inc', flags), ('under_test.inc', code))


@pytest.mark.parametrize('case', ['placed-dead', 'conversation', 'named', 'child'])
def test_object_names(case):
    status, output = run(build('object_names', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('case', ['placed-dead', 'conversation'])
def test_not_forgotten_fails(case):
    status, output = run(build('object_names', generated('not-forgotten')), case)
    assert status == CHECK_FAILED, output
