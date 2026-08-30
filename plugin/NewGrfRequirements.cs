using System;
using System.Collections.Generic;
using System.Linq;

namespace LauncherV2.Plugins.OpenTTD;

// Checks the player's own NewGRF sets against what the seed was generated
// for. Advisory only — the player supplies their own content.

/// How an installed set measures up against what the seed asked for.
public enum NewGrfState
{
    /// Installed, and new enough.
    Satisfied,
    /// Installed, but older than the seed was generated against.
    TooOld,
    /// Not installed at all.
    Missing,
    /// ⚠⚠ The file IS on disk — the game simply has not been told to load it.
    ///
    /// Without this state the launcher called a downloaded set "MISSING" and
    /// offered to download it again, which is what somebody with all six sets
    /// sitting in content_download was told three times in a row. OpenTTD only
    /// loads a NewGRF once it is ticked in the game's own NewGRF window, and
    /// nothing the launcher downloads changes that.
    NotEnabled,
}

/// One set the seed depends on, as announced in slot_data.
/// GrfId: Lowercase hex, e.g. "43411223".
/// MinVersion: Action 14 VRSN build number, not the marketing version.
public sealed record NewGrfRequirement(string GrfId, string DisplayName, uint MinVersion);

/// The verdict for one requirement, ready to draw as a badge.
public sealed record NewGrfCheckResult(
    NewGrfRequirement Required,
    NewGrfInfo?       Installed,
    NewGrfState       State)
{
    public bool Ok => State == NewGrfState.Satisfied;

    public string Status => State switch
    {
        NewGrfState.Satisfied => Installed?.Version is { } v
            ? $"Installed (build {v})" : "Installed",
        NewGrfState.TooOld => $"Too old — build {Installed?.Version} installed, "
                            + $"build {Required.MinVersion} required",
        NewGrfState.NotEnabled => "You have it — it is just not enabled",
        _ => "MISSING — this seed needs it",
    };

    /// What the player should actually do. Null when nothing to do.
    public string? Advice => State switch
    {
        NewGrfState.Satisfied => null,
        NewGrfState.TooOld =>
            $"Open OpenTTD, go to Check Online Content, and update {Required.DisplayName}."
            + SourceHint(),
        NewGrfState.NotEnabled =>
            $"You already have {Required.DisplayName}. Open OpenTTD, press NewGRF "
            + "Settings on the title screen, move it to the active list, and "
            + "restart the game — nothing needs downloading.",
        _ =>
            $"Open OpenTTD, go to Check Online Content, and install {Required.DisplayName}."
            + SourceHint(),
    };

    // Several sets state their own home page inside the file. When the player
    // already has an older copy we can point at exactly where the right version
    // lives, without us keeping a link list of our own.
    private string SourceHint()
        => Installed?.Url is { Length: > 0 } u ? $" The set's own page is {u}" : "";
}

public static class NewGrfRequirements
{
    ///
    /// Compare what is installed against what the seed needs.
    /// Requirements come from slot_data — this table is not a catalogue we
    /// maintain, it is whatever the generator said it used.
    ///
    /// <param name="installed">What the GAME reports it has loaded.</param>
    /// <param name="onDisk">What a scan of the game's folders can see. ⚠ Not
    /// the same question: OpenTTD loads a NewGRF only once it is ticked in its
    /// own NewGRF window, so a set can be downloaded, present, and still not
    /// loaded. Without this the launcher called such a set MISSING and offered
    /// to fetch it again — with all six already sitting in
    /// content_download.</param>
    public static IReadOnlyList<NewGrfCheckResult> Evaluate(
        IEnumerable<NewGrfInfo> installed,
        IEnumerable<NewGrfRequirement> required,
        IEnumerable<NewGrfInfo>? onDisk = null)
    {
        var have = installed.Where(g => g.GrfId != null)
                            .ToDictionary(g => g.GrfId!, StringComparer.OrdinalIgnoreCase);
        var files = (onDisk ?? Array.Empty<NewGrfInfo>())
                    .Where(g => g.GrfId != null)
                    .GroupBy(g => g.GrfId!, StringComparer.OrdinalIgnoreCase)
                    .ToDictionary(g => g.Key, g => g.First(),
                                  StringComparer.OrdinalIgnoreCase);

        var results = new List<NewGrfCheckResult>();
        foreach (var req in required)
        {
            have.TryGetValue(req.GrfId, out NewGrfInfo? got);

            // On disk but not loaded is its own answer, and a different job
            // for the player: tick it, do not download it.
            if (got == null && files.TryGetValue(req.GrfId, out var file))
            {
                results.Add(new NewGrfCheckResult(req, file, NewGrfState.NotEnabled));
                continue;
            }

            NewGrfState state =
                got == null                                  ? NewGrfState.Missing
              : req.MinVersion == 0                          ? NewGrfState.Satisfied
              : got.Version is { } v && v >= req.MinVersion   ? NewGrfState.Satisfied
              // A set that does not state a version cannot be proven too old.
              // Refusing to launch over a missing field would block players
              // whose install is fine, so an unversioned set passes.
              : got.Version == null                          ? NewGrfState.Satisfied
                                                             : NewGrfState.TooOld;

            results.Add(new NewGrfCheckResult(req, got, state));
        }
        return results;
    }

    /// Everything that would stop the game from being playable.
    public static IReadOnlyList<NewGrfCheckResult> Blockers(
        IEnumerable<NewGrfCheckResult> results)
        => results.Where(r => !r.Ok).ToList();

    ///
    /// One message for the player covering every problem at once. Null when
    /// there is nothing wrong — a launch is not the place for "all clear".
    ///
    public static string? Explain(IEnumerable<NewGrfCheckResult> results)
    {
        var bad = Blockers(results);
        if (bad.Count == 0) return null;

        string head = bad.Count == 1
            ? "This seed needs a NewGRF you do not have:"
            : $"This seed needs {bad.Count} NewGRFs you do not have:";

        return head + Environment.NewLine + Environment.NewLine
             + string.Join(Environment.NewLine + Environment.NewLine,
                   bad.Select(r => $"{r.Required.DisplayName}: {r.Status}"
                                 + (r.Advice != null ? Environment.NewLine + r.Advice : "")));
    }

    ///
    /// Read requirements out of an apworld slot_data block.
    /// Expected shape, one entry per set the generator drew items from:
    ///     "required_newgrf": [ { "grfid": "43411223",
    ///                            "name":  "Iron Horse",
    ///                            "min_version": 8948 } ]
    /// Anything malformed is skipped rather than thrown on: a seed with one
    /// unreadable entry should still tell the player about the other three.
    ///
    public static IReadOnlyList<NewGrfRequirement> FromSlotData(
        System.Text.Json.JsonElement slotData)
    {
        var list = new List<NewGrfRequirement>();
        if (slotData.ValueKind != System.Text.Json.JsonValueKind.Object) return list;
        if (!slotData.TryGetProperty("required_newgrf", out var arr)) return list;
        if (arr.ValueKind != System.Text.Json.JsonValueKind.Array) return list;

        foreach (var e in arr.EnumerateArray())
        {
            if (e.ValueKind != System.Text.Json.JsonValueKind.Object) continue;

            string? id = e.TryGetProperty("grfid", out var g) ? g.GetString() : null;
            if (string.IsNullOrWhiteSpace(id)) continue;

            string name = e.TryGetProperty("name", out var n) ? n.GetString() ?? id! : id!;
            uint min = e.TryGetProperty("min_version", out var v)
                    && v.ValueKind == System.Text.Json.JsonValueKind.Number
                    && v.TryGetUInt32(out uint u) ? u : 0;

            list.Add(new NewGrfRequirement(id!.Trim().ToLowerInvariant(), name, min));
        }
        return list;
    }
}
