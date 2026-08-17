# -*- coding: utf-8 -*-
"""A launcher, for the pipe client to be developed against.

The C++ side cannot be tested against London until London's OpenTTD plugin
exists, and that plugin cannot be finished until the C++ side speaks. This
breaks the deadlock: it is the launcher end of docs/ap_pipe_protocol.md, in
about a page, so the game can be developed and debugged on its own.

    python tools/pipe_stub.py [--name openttd_ap_stub] [options]

    --reject            answer the GRF list with REJECT: instead of SLOTDATA,
                        to exercise the launcher-side refusal
    --require-missing   claim the seed needs a set the game does not have, to
                        exercise the game's own check on slot_data
    --slotdata FILE     replay a real slot_data captured from a generated seed
                        instead of the minimal made-up one
    --standalone        no AP server anywhere: place items locally from the
                        seed's own tables and answer checks from that. This is
                        what London does when there is no multiworld.
    --seed N            placement seed for --standalone (default 1)
"""
import argparse
import json
import random
import sys
import time

try:
    import win32file
    import win32pipe
    import pywintypes
except ImportError:
    print("needs pywin32:  pip install pywin32")
    sys.exit(2)


def send(handle, line):
    win32file.WriteFile(handle, (line + "\n").encode("utf-8"))
    print("  -> " + line[:100])


def location_names(slot):
    """Every location the seed has, in the order the apworld built them."""
    names = [m["location"] for m in slot.get("missions", [])]
    names += list(slot.get("ruin_locations", []))
    names += list(slot.get("star_locations", []))
    names += sorted(slot.get("shop_item_names", {}).keys())
    names += [d["location"] for d in slot.get("demigods", [])]
    return names


class LocalSeed:
    """The whole multiworld, when there is no multiworld.

    Standalone is not a game feature: the game speaks the same protocol either
    way. Somebody has to answer, so this places the item pool on the seed's own
    locations and answers checks from that -- the job London's plugin does when
    the player picks a local seed.
    """

    def __init__(self, slot, seed):
        self.names = location_names(slot)
        pool = sorted(int(k) for k in slot.get("item_id_to_name", {}))
        rng = random.Random(seed)
        # One item per location, drawn with replacement: real generation obeys
        # logic, and logic is the apworld's job, not this file's.
        self.placement = {n: rng.choice(pool) for n in self.names} if pool else {}
        self.item_names = slot.get("item_id_to_name", {})
        self.index = 0

    def take(self, location_name):
        """@return (item id, resume index), or None for a location we do not know."""
        item = self.placement.get(location_name)
        if item is None:
            return None
        self.index += 1
        return item, self.index - 1

    def label(self, location_name):
        item = self.placement.get(location_name)
        name = self.item_names.get(str(item), "?") if item is not None else "?"
        return "Marco (OpenTTD) - " + name


def minimal_slot_data(grfs, require_missing):
    required = [{"grfid": g, "name": "set " + g, "min_version": 0} for g, _ in grfs]
    if require_missing:
        # Iron Horse's real id, at a build nobody has.
        required.append({"grfid": "43411223", "name": "Iron Horse",
                         "min_version": 99999})
    return {
        "required_newgrf": required,
        "goal": 0,
        "item_id_to_name": {"1": "Test Item"},
    }


def serve(args):
    path = r"\\.\pipe" + "\\" + args.name
    print("waiting on " + path)
    print("start the game with:  openttd.exe -ap-pipe " + args.name)
    print()

    real = None
    if args.slotdata:
        real = json.load(open(args.slotdata, encoding="utf-8"))
        print("replaying %s: %d keys, %d locations"
              % (args.slotdata, len(real), len(location_names(real))))

    handle = win32pipe.CreateNamedPipe(
        path,
        win32pipe.PIPE_ACCESS_DUPLEX,
        win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_READMODE_BYTE | win32pipe.PIPE_WAIT,
        1, 65536, 65536, 0, None)

    win32pipe.ConnectNamedPipe(handle, None)
    print("game connected")

    grfs = []
    local = None
    buf = b""
    answered = False

    while True:
        try:
            _, chunk = win32file.ReadFile(handle, 65536)
        except pywintypes.error:
            print("game disconnected")
            return
        if not chunk:
            time.sleep(0.05)
            continue

        buf += chunk
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            line = raw.decode("utf-8", "replace").strip()
            if not line:
                continue
            print("  <- " + line[:100])

            if line.startswith("GRF:"):
                parts = line.split(":", 2)
                if len(parts) == 3:
                    grfs.append((parts[1], parts[2]))

            elif line == "GRFEND:" and not answered:
                answered = True
                print("     loaded sets: %s" % (", ".join(g for g, _ in grfs) or "none"))
                if args.reject:
                    send(handle, "REJECT:This seed needs Iron Horse 4.14.1. "
                                 "Open Check Online Content, install it, then reconnect.")
                    continue

                slot = real if real is not None else minimal_slot_data(grfs, args.require_missing)
                names = location_names(slot)
                if args.standalone:
                    local = LocalSeed(slot, args.seed)
                    print("     standalone: %d locations placed from seed %d"
                          % (len(local.placement), args.seed))

                send(handle, "SLOTDATA:" + json.dumps(slot, separators=(",", ":")))
                send(handle, "LOCCOUNT:%d" % (len(names) or 5))
                send(handle, "MISSING:" + (",".join(str(i) for i in range(1, len(names) + 1))
                                           if names else "1,2,3,4,5"))
                send(handle, "PLAYERS:Marco" if args.standalone else "PLAYERS:Marco,Maegis")
                send(handle, "STATE:2")

            elif line.startswith("CHECKNAME:"):
                name = line.split(":", 1)[1]
                if local is not None:
                    got = local.take(name)
                    if got is None:
                        print("     ! unknown location: " + name)
                    else:
                        send(handle, "ITEM:%d:%d" % got)
                else:
                    send(handle, "ITEM:1:0")

            elif line.startswith("CHECK:"):
                send(handle, "ITEM:1:0")

            elif line == "SCOUT:":
                # Shop slots ask what they contain. Only standalone knows.
                if local is not None:
                    shop = [n for n in local.names if n.startswith("Shop_Purchase_")]
                    for n in shop:
                        send(handle, "HINT:%s:%s" % (n, local.label(n)))
                    print("     %d shop slots labelled" % len(shop))

            elif line == "GOAL:":
                print("     *** goal reached ***")

            elif line.startswith("DEATH:"):
                if not args.standalone:
                    send(handle, "DEATHLINK:" + line.split(":", 1)[1])


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", default="openttd_ap_stub")
    ap.add_argument("--reject", action="store_true")
    ap.add_argument("--require-missing", action="store_true")
    ap.add_argument("--slotdata")
    ap.add_argument("--standalone", action="store_true")
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    try:
        serve(a)
    except KeyboardInterrupt:
        print("stopped")
