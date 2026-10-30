"""The real lruv_block_new_moving (lruv_cache.c): a request no run of pages unused this tick can hold is placed by moving
this tick's blocks with their bytes, never a locked one (a large Custom Edition map's texture cache in co-op)."""

import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, mutated, read, run, structure  # noqa: E402

CASES = ["moves-used-blocks", "no-move-when-a-run-is-free", "keeps-locked-blocks", "none-when-all-used",
         "skips-a-failed-size-this-tick", "none-when-too-large"]

# a fault in the code and the case that must catch it
NEGATIVE_CONTROLS = {
    "plans-no-moves": (("if (inside && segment->state == _lruv_segment_used)", "if (0)"), "moves-used-blocks"),
    "bytes-not-copied": (("\tmove_block_proc(\n", "\tif (0) move_block_proc(\n"), "moves-used-blocks"),
    "moves-locked-blocks": (("if (!locked_count)", "if (locked_count >= 0)"), "keeps-locked-blocks"),
    "plans-a-failed-size-again": (("failed_cache = cache;", "failed_cache = NULL;"), "skips-a-failed-size-this-tick"),
}

FUNCTIONS = ["lruv_cache_bytes_to_pages", "lruv_cache_verify", "lruv_block_delete", "lruv_block_new",
             "lruv_cache_segments", "lruv_cache_plan_moves", "lruv_cache_clear_pages", "lruv_cache_move_block",
             "lruv_block_new_moving"]


def macro(source, name):
    """A #define with its continuation lines."""
    match = re.search(r"^#define " + re.escape(name) + r"\b(?:.*\\\n)*.*\n", source, re.M)
    if not match:
        raise LookupError(f"macro not found in the sources: {name}")
    return match.group(0)


def typedef(source, name):
    match = re.search(r"^typedef [^;]*\(\*" + re.escape(name) + r"\)\([^;]*\);", source, re.M)
    if not match:
        raise LookupError(f"typedef not found in the sources: {name}")
    return match.group(0)


def generated(fault=None):
    header = read("source/memory/lruv_cache.h")
    source = read("source/memory/lruv_cache.c")
    parts = [typedef(header, name) for name in
             ("lruv_delete_block_proc", "lruv_locked_block_proc", "lruv_move_block_proc")]
    parts += [structure(header, "lruv_cache"), structure(header, "lruv_cache_block"),
              enum_with(source, "MAXIMUM_LRUV_CACHE_HOLES") + ";", macro(source, "NEXT_HOLE_INDEX"),
              macro(source, "lruv_cache_block_get"), structure(source, "lruv_cache_hole"),
              enum_with(source, "_lruv_segment_free") + ";", structure(source, "lruv_cache_segment"),
              structure(source, "lruv_cache_range")]
    parts += ["static void lruv_cache_verify(struct lruv_cache *cache, boolean verify_blocks);",
              "static long lruv_cache_bytes_to_pages(struct lruv_cache *cache, long size);"]
    code = "\n".join(function(source, name) for name in FUNCTIONS)
    if fault:
        code = mutated(code, *fault)
    return (("under_test.inc", "\n".join(parts) + "\n" + code + "\n"),)


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("lruv_moving", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("lruv_moving", generated(fault)), case)
    assert status == CHECK_FAILED, f"lruv_block_new_moving with a fault ({control}) passed '{case}': the test cannot see it"
