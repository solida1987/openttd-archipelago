using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace LauncherV2.Plugins.OpenTTD;

/// Ticking a NewGRF for the player, in OpenTTD's own config.
///
/// ⚠⚠ WHY THIS EXISTS
///
/// A seed generated with SHARK, Vactrain and Hover Vehicles is refused unless
/// the game has LOADED them, and OpenTTD loads a NewGRF only once it is in the
/// `[newgrf]` group of its config. A player with all three sitting in
/// content_download was therefore stuck in a loop: download them (already
/// there), launch, get refused, repeat. Nothing London could download would
/// ever have helped.
///
/// ⭐ The format is read out of OpenTTD's own settings.cpp, not guessed:
///
///     GRFSaveConfig  writes  "{grfid:08X}|{md5}|{filename}"
///     GRFLoadConfig  parses  grfid, then md5, then the filename — and when
///                            the md5 does not parse and the filename is not a
///                            file, it falls back to
///                            FindGRFConfig(grfid, FGCM_NEWEST_VALID)
///
/// That fallback is the whole trick: an entry needs only a GRF ID, and the
/// game resolves it to the newest copy the player actually has — loose .grf or
/// inside a .tar, it does not matter.
public static class NewGrfEnabler
{
    /// The config OpenTTD will read.
    ///
    /// ⚠ Two candidates, in the game's own order: a config beside the exe wins
    /// (that is what makes an install portable), otherwise the personal folder
    /// under Documents. GetFolderPath follows OneDrive's redirection, which is
    /// where this player's real config turned out to live — a hard-coded
    /// %USERPROFILE%\Documents would have written a file the game never reads.
    public static string? FindConfig(string gameDirectory)
    {
        try
        {
            // ⚠⚠ MEASURED, not assumed. This build keeps its personal folder
            // at <game>\data\ -- the live config is <game>\data\openttd.cfg
            // and its downloads land in <game>\data\content_download. An
            // earlier version of this looked beside the exe and then under
            // Documents, found a stale file in a OneDrive folder, and drew a
            // long and completely wrong conclusion from it.
            //
            // Ordered by how the game itself resolves them: its own data
            // folder, then beside the exe (a portable install), then the
            // Documents folder a stock build would use.
            foreach (string candidate in new[]
            {
                Path.Combine(gameDirectory, "data", "openttd.cfg"),
                Path.Combine(gameDirectory, "openttd.cfg"),
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
                             "OpenTTD", "openttd.cfg"),
            })
                if (File.Exists(candidate)) return candidate;
        }
        catch (Exception) { /* no config we can name */ }
        return null;
    }

    /// The config to write into, created when the game has never run.
    ///
    /// ⚠⚠ A FRESH INSTALL HAS NO openttd.cfg AT ALL. The game writes one when
    /// it exits, so before the first session there is no file for
    /// <see cref="Enable"/> to add to -- and openttd.cfg is the only NewGRF
    /// list that survives the scan (see <see cref="OwnGrfs"/>). Bailing out
    /// there meant the mod's own ruin and star sets were off for the first
    /// seed a player ever generated, which is the one with no ruins in it.
    ///
    /// The file holds nothing but the empty group: every other setting stays
    /// at the game's default, and the game rewrites the file in full on exit.
    public static string? EnsureConfig(string gameDirectory)
    {
        string? found = FindConfig(gameDirectory);
        if (found != null) return found;

        // The path FindConfig prefers, so the next run finds this same file.
        string path = Path.Combine(gameDirectory, "data", "openttd.cfg");
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            string tmp = path + ".tmp";
            File.WriteAllLines(tmp, new[] { "[newgrf]", "" }, new UTF8Encoding(false));
            File.Move(tmp, path, overwrite: true);
            return path;
        }
        catch (Exception) { return null; }
    }

    /// The mod's OWN NewGRFs, which every session needs whatever the seed says.
    ///
    /// ⚠⚠ WHY THIS IS A NAMED, TESTABLE THING AND NOT TWO LINES IN A CALLER
    ///
    /// These define the map objects a ruin and a star are drawn as. The game
    /// adds them to _grfconfig_newgame itself, and then loses them again:
    /// AfterNewGRFScan::OnNewGRFsScanned calls LoadFromConfig, which replaces
    /// that list with whatever openttd.cfg holds. The scan is asynchronous, so
    /// it lands after the mod has made its choice.
    ///
    /// openttd.cfg is therefore the only list that survives — and the one time
    /// this lived as two lines inside PrepareNewGrfConfig, it sat AFTER an
    /// early return that fired whenever a seed required no NewGRF of its own.
    /// A player with a 400-ruin pool got no ruins at all:
    ///     [AP] WARNING: No ruin ObjectTypes found! GRFID=0x55525041
    ///
    /// Ids measured from the files by the packaging gate
    /// (tools/lint_no_foreign_grf.py), never guessed.
    public static readonly (string GrfId, string File)[] OwnGrfs =
    {
        ("41505255", "archipelago_ruins.grf"),
        ("41505354", "archipelago_stars.grf"),
    };

    /// Those of <see cref="OwnGrfs"/> that are actually in this install.
    ///
    /// Named relative to the newgrf folder, which is where the game package
    /// puts them. One that is missing is reported by its absence rather than
    /// written as an entry pointing at nothing.
    public static List<(string GrfId, string Name)> OwnSets(string gameDirectory)
    {
        var found = new List<(string, string)>();
        foreach (var (id, file) in OwnGrfs)
        {
            try
            {
                if (File.Exists(Path.Combine(gameDirectory, "newgrf", file)))
                    found.Add((id, file));
            }
            catch (Exception) { /* an unreadable folder is a missing file */ }
        }
        return found;
    }

    /// Add these sets to the config's `[newgrf]` group.
    ///
    /// Returns null when the file now lists every one of them, otherwise a
    /// sentence for the player. Never throws.
    ///
    /// ⚠ ADDS. Whatever the player already had stays exactly where it was, in
    /// its own order — this is their file, and a seed needing three sets is no
    /// reason to drop the twenty they chose themselves.
    public static string? Enable(string configPath,
                                 IEnumerable<(string GrfId, string Name)> sets,
                                 Action<string>? log = null)
    {
        var wanted = sets.Where(s => s.GrfId is { Length: 8 }).ToList();
        if (wanted.Count == 0) return null;

        List<string> lines;
        try { lines = File.ReadAllLines(configPath).ToList(); }
        catch (Exception e) { return $"Could not read {configPath}: {e.Message}"; }

        // Where does [newgrf] start and end? A missing group is normal: a
        // player who has never enabled one has no such section at all, which
        // is exactly the case this was written for.
        int start = lines.FindIndex(l => l.Trim()
            .Equals("[newgrf]", StringComparison.OrdinalIgnoreCase));
        int end;
        if (start < 0)
        {
            lines.Add("");
            lines.Add("[newgrf]");
            start = lines.Count - 1;
            end = lines.Count;
        }
        else
        {
            end = start + 1;
            while (end < lines.Count && !lines[end].TrimStart().StartsWith("[")) end++;
        }

        var existing = lines.Skip(start + 1).Take(end - start - 1)
                            .Select(l => l.Split('|')[0].Trim())
                            .Where(s => s.Length > 0)
                            .ToHashSet(StringComparer.OrdinalIgnoreCase);

        var added = new List<string>();
        foreach (var (id, name) in wanted)
        {
            string key = id.ToUpperInvariant();
            if (existing.Contains(key)) continue;
            // ⚠ The middle field is deliberately not an md5. An md5 would pin
            // the entry to one exact build, and the player's copy is whatever
            // version the content service last handed them. Leaving it
            // unparseable is what sends OpenTTD down the "newest valid copy of
            // this GRF ID" path in GRFLoadConfig.
            // ⚠⚠ The FILENAME is what makes the entry resolve.
            //
            // GRFLoadConfig only falls back to FindGRFConfig(grfid) when the
            // named file does NOT exist -- and at the moment settings load,
            // the NewGRF scan has not run, so that fallback finds nothing and
            // the entry ends up GCS_NOT_FOUND. Measured: entries written with
            // a placeholder name gave "game loaded 0" every time.
            //
            // With a real name, FioCheckFileExists succeeds and the scan fills
            // the rest in. For a set inside a .tar the game spells that
            // "<archive>.tar\<member path>", lowercased -- exactly what its own
            // NewGRF window shows.
            lines.Insert(end++, $"{key}|-|{name} =");
            added.Add(name);
        }
        if (added.Count == 0) return null;

        try
        {
            // The player's own file: keep a copy before touching it.
            string backup = configPath + ".london-backup";
            if (!File.Exists(backup)) File.Copy(configPath, backup);

            string tmp = configPath + ".tmp";
            File.WriteAllLines(tmp, lines, new UTF8Encoding(false));
            File.Move(tmp, configPath, overwrite: true);
        }
        catch (Exception e) { return $"Could not write {configPath}: {e.Message}"; }

        log?.Invoke($"enabled in openttd.cfg: {string.Join(", ", added)}");
        return null;
    }

    /// The name OpenTTD writes for a set, relative to its newgrf folder.
    ///
    /// ⚠ A set inside a .tar is spelled "<archive>.tar\<member path>",
    /// lowercased -- exactly what the game's own NewGRF window shows. Our
    /// scanner records the same thing as "<absolute tar path>!<member>", so
    /// this is the translation between the two.
    ///
    /// The filename matters: GRFLoadConfig only resolves an entry by GRF id
    /// when the named file does NOT exist, and at settings-load time the
    /// NewGRF scan has not run, so that path finds nothing. With a real name
    /// the entry resolves and the scan fills in the rest.
    public static string GameRelativeName(string scannerPath)
    {
        int bang = scannerPath.IndexOf('!');
        if (bang < 0) return Path.GetFileName(scannerPath);
        string archive = Path.GetFileName(scannerPath[..bang]);
        string member = scannerPath[(bang + 1)..].Replace('/', '\\');
        return (archive + "\\" + member).ToLowerInvariant();
    }

    /// Is this set already listed in the config?
    public static bool IsEnabled(string configPath, string grfId)
    {
        try
        {
            bool inGroup = false;
            foreach (string raw in File.ReadLines(configPath))
            {
                string l = raw.Trim();
                if (l.StartsWith("["))
                {
                    inGroup = l.Equals("[newgrf]", StringComparison.OrdinalIgnoreCase);
                    continue;
                }
                if (inGroup && l.Split('|')[0].Trim()
                        .Equals(grfId, StringComparison.OrdinalIgnoreCase))
                    return true;
            }
        }
        catch (Exception) { /* unreadable: not enabled as far as we can tell */ }
        return false;
    }
}
