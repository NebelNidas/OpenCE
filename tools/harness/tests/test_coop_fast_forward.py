"""A co-op host's count of the machines holding accept to fast-forward a scene (network_coop.c)."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run, structure

CASES = ["majority", "even", "dedicated", "silent", "scene", "status", "outside"]
NEGATIVE_CONTROLS = {
    "half-is-enough": (("fast_forward.holders * 2 > fast_forward.machines", "fast_forward.holders * 2 >= fast_forward.machines"), "even"),
    "counts-a-dedicated-host": (("if (local_player_get_next(NONE) != NONE)", "if (TRUE)"), "dedicated"),
    "counts-silent-clients": (("now - fast_forward.client_heard_times[machine_index] > FAST_FORWARD_HELD_MILLISECONDS",
                               "FALSE"), "silent"),
}


def anonymous(source, name):
    """A file-scope `static struct { ... } name;`."""
    end = source.index("} " + name + ";")
    return source[source.rindex("static struct", 0, end):end + len(name) + 3]


def generated(fault=None):
    coop = read("port/linux/game/network_coop.c")
    types = structure(coop, "distributed_coop_skip_vote") + "\n" + anonymous(coop, "skip_vote") + "\n" + \
        anonymous(coop, "fast_forward") + "\n"
    code = ""
    for name in ("coop_host", "coop_client", "network_coop_fast_forward_hold", "network_coop_fast_forward_status",
                 "network_coop_handle_skip_vote", "host_count_fast_forward"):
        text = function(coop, name)
        if fault and name == "host_count_fast_forward":
            text = mutated(text, *fault)
        code += text + "\n"
    return (("types.inc", types), ("under_test.inc", code))


@pytest.mark.parametrize("case", CASES)
def test_fast_forward_vote(case):
    status, output = run(build("coop_fast_forward", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("name", NEGATIVE_CONTROLS)
def test_negative_control(name):
    fault, case = NEGATIVE_CONTROLS[name]
    status, output = run(build("coop_fast_forward", generated(fault)), case)
    assert status == CHECK_FAILED, output
