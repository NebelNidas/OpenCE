"""A particle tint interpolated in hsv (source/bitmaps/bitmap_utilities.c): hue exactly 1 is hue 0."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, inline, mutated, read, run


def union(source, name):
    return re.search(r"^union " + re.escape(name) + r"\n\{.*?\n\};", source, re.S | re.M).group(0)


def generated(fault=None):
    cseries = read('source/cseries/cseries.h')
    macros = '\n'.join(re.search(r"^#define " + name + r"\(.*$", cseries, re.M).group(0) for name in ('CEILING', 'PIN'))
    math = read('source/math/real_math.h')
    header = read('source/bitmaps/bitmap_utilities.h')
    source = read('source/bitmaps/bitmap_utilities.c')
    code = '\n'.join([union(math, 'real_rgb_color'), union(header, 'real_hsv_color'),
                      enum_with(source, '_rgb_color_interpolation_hsv_bit'), inline(math, 'valid_real'),
                      function(source, 'valid_real_rgb_color'),
                      function(source, 'real_rgb_color_to_real_hsv_color'),
                      function(source, 'real_hsv_color_to_real_rgb_color'),
                      function(source, 'rgb_colors_interpolate')])
    if fault:
        code = mutated(code, *fault)
    return (('macros.inc', macros + '\n'), ('under_test.inc', code))


@pytest.mark.parametrize('case', ['hue_one', 'between'])
def test_case(case):
    status, output = run(build('hsv_interpolation', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('fault,case', [
    (('\tcase 6:\n', ''), 'hue_one'),
])
def test_negative_control(fault, case):
    status, output = run(build('hsv_interpolation', generated(fault)), case)
    assert status == CHECK_FAILED, output
