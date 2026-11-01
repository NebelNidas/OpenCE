"""A co-op player who left: the real quit, any_player_is_dead and game_safe_to_speak."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run


def generated(fault=None):
    players = read('source/game/players.c')
    game = read('source/game/game.c')
    dead = function(players, 'any_player_is_dead')
    if fault == 'quit-counted':
        dead = mutated(dead, '!player->quit_out_of_game &&', '')
    code = (function(players, 'players_coop_waiting_to_start') + '\n' + dead + '\n' +
            function(game, 'game_safe_to_speak') + '\n' + function(game, 'remove_quitting_players_from_game'))
    return (('under_test.inc', code),)


@pytest.mark.parametrize('case', ['quit', 'quit-later', 'dead', 'waiting', 'projectiles'])
def test_quit_speak(case):
    status, output = run(build('coop_quit_speak', generated()), case)
    assert status == 0, output


def test_quit_counted_fails():
    status, output = run(build('coop_quit_speak', generated('quit-counted')), 'quit')
    assert status == CHECK_FAILED, output
