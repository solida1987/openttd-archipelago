# -*- coding: utf-8 -*-
"""Export the apworld's option schema for London's YAML generator.

The launcher lets a player build a settings file without opening a text
editor, and the only way that stays honest is to take the option list from the
apworld itself. A YAML carrying a key the apworld does not know fails at
generation time with a message that means nothing to the player.

Reading the file with a regex is not enough: options.py imports Archipelago's
Options module and declares its fields as dataclass annotations, so the file
has to actually run. It only needs a handful of names from Archipelago, so we
stand those up as stubs that record what each class declares, import options.py
under them, and read the dataclass and OPTION_GROUPS back out.

    python tools/gen_yaml_options.py

Writes OpenTTD-London-Plugin/OpenTTDYamlOptions.g.cs.
RE-RUN whenever options.py changes.
"""
import io
import os
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
APWORLD = os.path.join(ROOT, "apworld", "openttd")
OUT = os.path.join(os.path.dirname(ROOT), "OpenTTD-London-Plugin",
                   "OpenTTDYamlOptions.g.cs")

# The stable line renames the game; the launcher writes whichever it is told.
GAME_NAME = "OpenTTD"


# ---- stand-ins for Archipelago's Options module -----------------------------
# Only what options.py touches. The real classes do far more, none of which
# matters for describing a form.

class _Base:
    default = 0
    display_name = ""
    visibility = None

    @classmethod
    def describe(cls):
        doc = (cls.__doc__ or "").strip().split("\n")
        summary = " ".join(l.strip() for l in doc if l.strip())
        return {
            "display": getattr(cls, "display_name", "") or cls.__name__,
            "default": getattr(cls, "default", 0),
            "help": summary,  # full text; the dialog wraps, it never trims
        }


class Toggle(_Base):
    kind = "toggle"


class DefaultOnToggle(Toggle):
    default = 1


class DeathLink(Toggle):
    display_name = "Death Link"


class Range(_Base):
    kind = "range"
    range_start = 0
    range_end = 100


class NamedRange(Range):
    pass


class Choice(_Base):
    kind = "choice"


class TextChoice(Choice):
    pass


class OptionSet(_Base):
    kind = "set"
    default = ()


class FreeText(_Base):
    kind = "text"
    default = ""


class PerGameCommonOptions:
    pass


class OptionGroup:
    def __init__(self, name, options, start_collapsed=False):
        self.name = name
        self.options = options
        self.start_collapsed = start_collapsed


class Visibility:
    """options.py sets `visibility = Visibility.none` to hide an option."""
    none = 0
    template = 1
    simple_ui = 2
    complex_ui = 4
    spoiler = 8
    all = 15


_STUBS = ("Toggle", "DefaultOnToggle", "Range", "NamedRange", "Choice",
          "TextChoice", "OptionSet", "FreeText", "DeathLink",
          "PerGameCommonOptions", "OptionGroup", "Visibility")


def load_options():
    stub = types.ModuleType("Options")
    for name in _STUBS:
        setattr(stub, name, globals()[name])
    sys.modules["Options"] = stub
    sys.modules["BaseClasses"] = types.ModuleType("BaseClasses")
    sys.path.insert(0, APWORLD)
    import options as O   # noqa: E402
    return O


def fields(O):
    """(key, class) for every option, in declaration order."""
    for name in dir(O):
        cls = getattr(O, name)
        if isinstance(cls, type) and issubclass(cls, PerGameCommonOptions) \
                and cls is not PerGameCommonOptions:
            return list(getattr(cls, "__annotations__", {}).items())
    return []


def choice_values(cls):
    """option_<name> = <n> pairs, in declared order, prettied for display."""
    out = [(v, k[len("option_"):]) for k, v in vars(cls).items()
           if k.startswith("option_") and isinstance(v, int)]
    out.sort()
    return [(v, n.replace("_", " ").title()) for v, n in out]


def cs_string(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') \
                  .replace("\r", " ").replace("\n", " ") + '"'


def main():
    O = load_options()
    all_fields = fields(O)
    if not all_fields:
        print("could not find the options dataclass in options.py")
        return 1

    # key -> group, from the same groups the AP Options Creator renders.
    group_of = {}
    for g in getattr(O, "OPTION_GROUPS", []):
        for cls in g.options:
            group_of[cls.__name__] = g.name

    entries = []
    skipped = []
    for key, cls in all_fields:
        kind = getattr(cls, "kind", None)
        if kind is None:
            skipped.append((key, "not an option class"))
            continue
        if getattr(cls, "visibility", None) == Visibility.none:
            # Hidden in AP's own UI; offering it here would only confuse.
            skipped.append((key, "hidden"))
            continue
        if kind in ("set", "text"):
            # No sensible form control. The apworld's default applies.
            skipped.append((key, kind))
            continue

        d = cls.describe()
        e = {
            "key": key,
            "kind": kind,
            "display": d["display"],
            "help": d["help"],
            "group": group_of.get(cls.__name__, "Other"),
            "min": 0, "max": 0, "choices": [],
        }
        if kind == "range":
            e["min"] = getattr(cls, "range_start", 0)
            e["max"] = getattr(cls, "range_end", 100)
            e["default"] = int(d["default"])
        elif kind == "choice":
            e["choices"] = choice_values(cls)
            e["default"] = int(d["default"])
        else:
            e["default"] = 1 if d["default"] else 0
        entries.append(e)

    # Keep the AP group order, then anything ungrouped.
    order = [g.name for g in getattr(O, "OPTION_GROUPS", [])] + ["Other"]
    entries.sort(key=lambda e: order.index(e["group"]) if e["group"] in order else 99)

    lines = [
        "// <auto-generated> by tools/gen_yaml_options.py -- DO NOT EDIT.",
        "// Regenerate whenever the apworld's options.py changes; the launcher's",
        "// YAML generator must offer exactly the options the apworld accepts, or",
        "// the file it writes fails at generation time with a message that helps",
        "// nobody.",
        "namespace LauncherV2.Plugins.OpenTTD;",
        "",
        "// long, not int: company-value and cargo targets run to ten billion,",
        "// which is where the first attempt at this file stopped compiling.",
        "public sealed record OpenTTDYamlOption(",
        "    string Key, string Kind, string Display, string Help, string Group,",
        "    long Default, long Min, long Max, (int Value, string Label)[] Choices);",
        "",
        "public static class OpenTTDYamlOptions",
        "{",
        f"    public const string Game = {cs_string(GAME_NAME)};",
        "    public static readonly OpenTTDYamlOption[] All =",
        "    {",
    ]
    for e in entries:
        choices = ("System.Array.Empty<(int, string)>()" if not e["choices"]
                   else "new[]{ " + ", ".join(
                       "(%d, %s)" % (v, cs_string(l)) for v, l in e["choices"]) + " }")
        lines.append("        new(%s, %s, %s, %s, %s, %d, %d, %d, %s)," % (
            cs_string(e["key"]), cs_string(e["kind"]), cs_string(e["display"]),
            cs_string(e["help"]), cs_string(e["group"]),
            e["default"], e["min"], e["max"], choices))
    lines += ["    };", "}", ""]

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    io.open(OUT, "w", encoding="utf-8", newline="\n").write("\n".join(lines))

    print("%d options -> %s" % (len(entries), os.path.relpath(OUT, os.path.dirname(ROOT))))
    groups = {}
    for e in entries:
        groups[e["group"]] = groups.get(e["group"], 0) + 1
    for g in order:
        if g in groups:
            print("    %-32s %3d" % (g, groups[g]))
    if skipped:
        print("  skipped %d: %s" % (len(skipped),
              ", ".join("%s (%s)" % s for s in skipped[:8])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
