#!/usr/bin/env python3
"""Builds the 4-bar two-step drum-and-bass pattern on MiguelMoraleja's firmware
in the simulator and counts the user actions it takes.

Same target as dnb_two_step.sim (the stock-UI recipe): kick on steps 1 and 11,
snare on 5 and 13, hats in sixteenths, 4 bars of 4/4, quantize 1/16.

    miguel_two_step.py <DrumboySim> <card image> [out dir]

Starts the simulator headless on a throwaway copy of the card, drives it over
the control socket and stops it again. Needs a simulator whose `state` has the
fields euclid and beat.selected (the sim-v1.16 worktree in forks/miguel-sim).

Counting: a key press or a chord of keys held together is one key action. An
encoder is counted in value steps, the number of times the field changed. The
simulator needs about two of its `turn` detents per value step on these fields
(ENC_RES_M) plus one after a change of direction; how many clicks of the real
knob that is has not been checked on the unit.
"""

import os
import socket
import subprocess
import sys
import time

TICKS_16TH = 200
BAR = 16 * TICKS_16TH
BARS = 4
SOCKET = "mts.sock"                                             # relative: socket paths are limited to 107 bytes


class Sim:
    def __init__(self, binary, card, out_dir):
        os.makedirs(out_dir, exist_ok=True)
        self.cwd = out_dir
        os.chdir(out_dir)                                       # the client connects by relative path too
        path = SOCKET
        if os.path.exists(path):
            os.unlink(path)
        self.log = open("sim.log", "w")
        self.proc = subprocess.Popen(
            [binary, "--headless", "--card", card, "--card-copy", "--delay-scale", "0.2", "--control", SOCKET, "--out", "."],
            cwd=out_dir, stdout=self.log, stderr=subprocess.STDOUT)
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        for attempt in range(100):
            try:
                self.sock.connect(path)
                break
            except OSError:
                if attempt == 99:
                    self.proc.kill()                            # do not leave it running with its card copy
                    raise
                time.sleep(0.1)
        self.reader = self.sock.makefile("r")
        self.keys = 0                                           # key actions (single keys and chords)
        self.steps = 0                                          # encoder value steps
        self.detents = 0                                        # simulator detents sent
        self.section = {}

    def cmd(self, command):
        self.sock.sendall((command + "\n").encode())
        reply = self.reader.readline().rstrip("\n")
        if not reply.startswith("ok"):
            raise RuntimeError(f"{command!r}: {reply}")
        return reply[3:]

    def press(self, keys, hold_ms=None):
        self.cmd(f"press {keys}" + (f" {hold_ms}" if hold_ms else ""))
        self.cmd("wait-stable 200")
        self.keys += 1

    def turn_until(self, encoder, direction, read, target, limit=80):
        """One detent at a time until read() == target; counts the value changes."""
        value = read()
        for _ in range(limit):
            if value == target:
                return
            self.cmd(f"turn {encoder} {direction}")
            self.cmd("wait 150")
            self.detents += 1
            new = read()
            if new != value:
                self.steps += 1
                value = new
        raise RuntimeError(f"encoder {encoder} did not reach {target!r}, stuck at {value!r}")

    def euclid(self):
        steps, hits, rotate, grid, merge, accents = (int(x) for x in self.cmd("get euclid").strip('"').split(","))
        return {"steps": steps, "hits": hits, "rotate": rotate, "grid": grid, "merge": merge, "accents": accents}

    def track(self, number):
        """Start ticks of the active beats of a track, in list order."""
        beats = self.cmd(f"dump-track {number}").split("|")[0].split()
        return [int(b.split("-")[0]) for b in beats]

    def selected_tick(self, number):
        index = int(self.cmd("get beat.selected").strip('"'))
        beats = self.track(number)
        return beats[index] if 0 <= index < len(beats) else None

    def mark(self, name):
        """Closes a section: stores the actions since the previous mark."""
        done_keys = sum(s[0] for s in self.section.values())
        done_steps = sum(s[1] for s in self.section.values())
        self.section[name] = (self.keys - done_keys, self.steps - done_steps)

    def quit(self):
        try:
            self.sock.sendall(b"quit\n")
        except OSError:
            pass
        self.proc.wait(timeout=20)
        self.log.close()


def pattern(steps_in_bar):
    return sorted(bar * BAR + (step - 1) * TICKS_16TH for bar in range(BARS) for step in steps_in_bar)


def main(argv):
    if len(argv) < 2:
        print(__doc__.strip())
        return 2
    binary, card = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    out_dir = os.path.abspath(argv[2] if len(argv) > 2 else "miguel_two_step_out")
    sim = Sim(binary, card, out_dir)
    ok = True
    try:
        sim.cmd("wait-ready 30000")
        sim.cmd("wait-stable 500")

        # EUCLID encoders: 1 HITS, 4 ROTATE, 6 MODE. Layer Song encoders: 6 MOVE.
        hits = lambda: sim.euclid()["hits"]
        rotate = lambda: sim.euclid()["rotate"]
        merge = lambda: sim.euclid()["merge"]

        # Snare, layer 2: E(2,16) rotated by 4 = steps 5 and 13
        sim.press("LAYER_SONG+1")
        sim.press("LAYER_SONG")                                 # song page -> EUCLID
        assert sim.cmd("get menu").strip('"') == "EUCLID"
        sim.turn_until(1, -1, hits, 2)
        sim.turn_until(4, 1, rotate, 4)
        sim.cmd("screenshot snare_euclid.png")
        sim.mark("snare")

        # Hats, layer 3: 16 hits in 16 steps
        sim.press("LAYER_SONG+2")
        if sim.cmd("get menu").strip('"') != "EUCLID":
            sim.press("LAYER_SONG")
        sim.turn_until(1, -1, hits, 16)                         # 2 -> 1 -> 0 -> 16
        sim.mark("hats")

        # Kick, layer 1: E(1,16) at step 1, then a second visit merging E(1,16) rotated to step 11
        sim.press("LAYER_SONG+0")
        if sim.cmd("get menu").strip('"') != "EUCLID":
            sim.press("LAYER_SONG")
        sim.turn_until(1, 1, hits, 1)                           # 16 -> 0 -> 1
        sim.turn_until(4, -1, rotate, 0)
        for _ in range(3):                                      # EUCLID -> NOTE -> song -> EUCLID: a new visit keeps what is there
            sim.press("LAYER_SONG")
        assert sim.cmd("get menu").strip('"') == "EUCLID"
        sim.turn_until(6, 1, merge, 1)
        sim.turn_until(4, -1, rotate, 10)                       # 0 -> 15 -> ... -> 10
        sim.cmd("screenshot kick_euclid.png")
        sim.mark("kick")

        want = {1: pattern([1, 11]), 2: pattern([5, 13]), 3: pattern(range(1, 17))}
        for number, name in ((1, "kick"), (2, "snare"), (3, "hats")):
            got = sorted(sim.track(number))
            good = got == want[number]
            ok &= good
            print(f"{name:5} track {number}: {len(got):2} beats, {'matches' if good else 'DIFFERS: ' + str(got)}")

        # Variation the stock UI cannot do without recording: one extra kick, bar 4 step 8
        extra = 3 * BAR + 7 * TICKS_16TH
        sim.press("LAYER_SONG")                                 # EUCLID -> NOTE
        sim.press("LAYER_SONG")                                 # NOTE -> song page
        assert sim.cmd("get menu").strip('"') == "LAYER_SONG"
        before = 3 * BAR                                        # the kick on bar 4 step 1
        for _ in range(20):
            if sim.selected_tick(1) == before:
                break
            sim.press("LEFT")                                   # from the first beat LEFT wraps to the last one
        sim.press("ADD", 80)                                    # short press: a beat in the middle of the gap after it
        inserted = sim.selected_tick(1)
        direction = 1 if extra > inserted else -1
        sim.turn_until(6, direction, lambda: sim.selected_tick(1), extra)
        sim.cmd("screenshot kick_extra.png")
        sim.mark("extra kick")
        got = sorted(sim.track(1))
        good = got == sorted(want[1] + [extra])
        ok &= good
        print(f"extra kick at tick {extra} (inserted at {inserted}): {'matches' if good else 'DIFFERS: ' + str(got)}")

        print()
        for name, (keys, steps) in sim.section.items():
            print(f"{name:10} {keys:2} key actions, {steps:2} encoder steps")
        base_keys = sum(v[0] for k, v in sim.section.items() if k != "extra kick")
        base_steps = sum(v[1] for k, v in sim.section.items() if k != "extra kick")
        print(f"{'pattern':10} {base_keys:2} key actions, {base_steps:2} encoder steps (kick + snare + hats)")
        print(f"simulator detents sent: {sim.detents} for {sim.steps} encoder steps")
        print(f"firmware warnings: {sim.cmd('warnings')}")
    finally:
        sim.quit()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
