"""display.hud_inset_x and _y (hud_draw.c) and HUD POSITION's presses (hud_position.c), the real code against a fake
view and config.toml."""

import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run  # noqa: E402

CASES = ["in", "out", "half-the-view", "split-screen", "settings", "preview", "presses"]
INSET = ("hud_inset_preview", "hud_inset_get", "hud_inset_axis", "hud_inset_apply")
POSITION = ("hud_position_begin", "hud_position_end", "hud_position_move", "hud_position_reset")

# a fault in the code, the function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    "no-half-view-cap": (("\tif (inset > 0)\n\t\tinset = MIN(inset, MAX(0, (*high - *low - (view_high - view_low) / 2) / 2));\n", ""),
                         "hud_inset_axis", "half-the-view"),
    "out-past-the-view": (("\tif (inset < 0)\n\t\tinset = MAX(inset, -MAX(0, MIN(*low - view_low, view_high - *high)));\n", ""),
                          "hud_inset_axis", "out"),
    "middle-moves": (("MIN(*low - view_low, view_high - *high)", "MAX(*low - view_low, view_high - *high)"),
                     "hud_inset_axis", "split-screen"),
    "settings-not-pinned": (('PIN(config_integer("display.hud_inset_x"), -HUD_INSET_MAXIMUM, HUD_INSET_MAXIMUM)',
                             'config_integer("display.hud_inset_x")'), "hud_inset_get", "settings"),
    "preview-ignored": (("*x = hud_inset.previewing ? hud_inset.preview_x : hud_inset.x;", "*x = hud_inset.x;"),
                        "hud_inset_get", "preview"),
    "never-held": (("hud_position.presses++;", "hud_position.presses = 0;"), "hud_position_move", "presses"),
}


def _global(source, name):
    """A file's static struct global, its definition and initializer."""
    match = re.search(r"^static struct\n\{\n.*?^\} " + re.escape(name) + r" = [^;]*;\n", source, re.M | re.S)
    if not match:
        raise LookupError(f"global not found in the sources: {name}")
    return match.group(0)


def generated(fault=None, faulty_function=None):
    hud_draw = read("source/interface/hud_draw.c")
    position = read("port/linux/game/hud_position.c")
    text = _global(hud_draw, "hud_inset")
    text += "".join(re.findall(r"^#define HUD_POSITION_HELD_\w+ \w+\n", position, re.M))
    text += _global(position, "hud_position")
    for source, names in ((hud_draw, INSET), (position, POSITION)):
        for name in names:
            code = function(source, name)
            if fault and name == faulty_function:
                code = mutated(code, *fault)
            text += code + "\n"
    return (("under_test.inc", text),)


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("hud_inset", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, faulty_function, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("hud_inset", generated(fault, faulty_function)), case)
    assert status == CHECK_FAILED, f"the code with a fault ({control}) passed '{case}': the test cannot see it"


def test_default_none_and_drawn_on_every_view():
    # no inset unless set, applied around the HUD's drawing and taken back after it
    config = read("port/linux/src/port_config.c")
    assert '{ "display.hud_inset_x", _config_integer, "0", "HALO_HUD_INSET_X"' in config
    assert '{ "display.hud_inset_y", _config_integer, "0", "HALO_HUD_INSET_Y"' in config
    draw = function(read("source/interface/hud.c"), "hud_draw_screen")
    assert draw.index("hud_inset_apply(&render.camera.window_bounds);") < draw.index("rasterizer_hud_begin();")
    assert draw.index("rasterizer_hud_end();") < draw.index("render.camera.window_bounds = window_before_inset;")
