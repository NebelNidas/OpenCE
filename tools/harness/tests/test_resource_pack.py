"""The real resource packing (custom_edition_cache.c) for a resource map larger than its part of the combined offset
space: each packed resource is found again by offset, the space's end is kept, and a read may cross pieces."""

import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, mutated, read, run, structure  # noqa: E402

CASES = ["joins-continuing-resources", "refuses-past-the-limit", "finds-every-piece", "reads-across-pieces"]

# a fault in the packing and the case that must catch it
NEGATIVE_CONTROLS = {
    "no-limit": (("if (size > COMBINED_PACKED_LIMIT - offset)", "if (0)"), "refuses-past-the-limit"),
    "joins-other-file": (("last->type == type && ", ""), "joins-continuing-resources"),
    "finds-one-past": (("else if (offset - resource->offset >= resource->size)",
                        "else if (offset - resource->offset > resource->size)"), "finds-every-piece"),
    "reads-whole-piece": (("resource->offset + resource->size - at", "resource->size"), "reads-across-pieces"),
}


def define(source, name):
    match = re.search(r"#define\s+" + re.escape(name) + r"\s+(\w+)", source)
    if not match:
        raise LookupError(f"#define not found in the sources: {name}")
    return f"#define {name} {match.group(1)}\n"


def generated(fault=None):
    cache = read("port/linux/game/custom_edition_cache.c")
    formats = read("port/linux/game/cache_file_formats.h")
    config = "".join(define(cache, name) for name in ("COMBINED_BITMAPS_OFFSET", "COMBINED_PACKED_LIMIT"))
    config += define(formats, "CUSTOM_EDITION_RESOURCE_UNPLACED")
    config += enum_with(formats, "_resource_map_bitmaps") + "\n" + structure(cache, "packed_resource") + "\n"
    code = "\n".join(function(cache, name) for name in (
        "custom_edition_resource_pack", "custom_edition_packed_resource", "custom_edition_packed_read"))
    if fault:
        code = mutated(code, *fault)
    return (("config.inc", config), ("under_test.inc", code + "\n"))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("resource_pack", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("resource_pack", generated(fault)), case)
    assert status == CHECK_FAILED, f"the packing with a fault ({control}) passed '{case}': the test cannot see it"
