using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace LauncherV2.Plugins.OpenTTD;

// A standalone run is a REAL solo Archipelago seed, generated at build time by
// tools/gen_standalone_seeds.py and shipped with the game. Nothing here
// re-randomises anything; the launcher just answers the pipe from the file.
internal sealed class StandaloneSeed
{
    public required string Label;
    public required string SeedName;
    public required JsonElement SlotData;
    public required Dictionary<string, long> NameToId;
    public required Dictionary<long, string> IdToName;
    public required Dictionary<long, long> Placements;    // location id -> item id
    public required Dictionary<long, string> ItemNames;   // item id -> display name

    public static string SeedsDir(string gameDir) => Path.Combine(gameDir, "standalone_seeds");

    public static IReadOnlyList<string> ListLabels(string gameDir)
    {
        try
        {
            return Directory.GetFiles(SeedsDir(gameDir), "*.json")
                .Select(Path.GetFileNameWithoutExtension)
                .Where(n => n != null && n != "index")
                .Select(n => n!)
                .OrderBy(n => n, StringComparer.OrdinalIgnoreCase)
                .ToArray();
        }
        catch (IOException) { return Array.Empty<string>(); }
    }

    public static StandaloneSeed Load(string gameDir, string label)
    {
        string path = Path.Combine(SeedsDir(gameDir), label + ".json");
        using var doc = JsonDocument.Parse(File.ReadAllText(path));
        var root = doc.RootElement;

        var nameToId = new Dictionary<string, long>(StringComparer.Ordinal);
        foreach (var p in root.GetProperty("location_name_to_id").EnumerateObject())
            nameToId[p.Name] = p.Value.GetInt64();

        var itemNames = new Dictionary<long, string>();
        foreach (var p in root.GetProperty("item_id_to_name").EnumerateObject())
            itemNames[long.Parse(p.Name)] = p.Value.GetString() ?? "";

        var placements = new Dictionary<long, long>();
        foreach (var p in root.GetProperty("placements").EnumerateObject())
            placements[long.Parse(p.Name)] = p.Value.GetInt64();

        return new StandaloneSeed
        {
            Label     = label,
            SeedName  = root.GetProperty("seed_name").GetString() ?? label,
            SlotData  = root.GetProperty("slot_data").Clone(),
            NameToId  = nameToId,
            IdToName  = nameToId.ToDictionary(kv => kv.Value, kv => kv.Key),
            Placements = placements,
            ItemNames = itemNames,
        };
    }
}

// Which locations a standalone run has checked, in the order they happened.
// The order IS the item receive-index, so a restarted game replays the same
// stream and the engine's index dedup does the rest.
internal sealed class StandaloneState
{
    public List<long> CheckedInOrder { get; } = new();
    public bool GoalDone { get; set; }

    private readonly string _path;
    private StandaloneState(string path) => _path = path;

    public static StandaloneState Load(string gameDir, string label)
    {
        string dir = Path.Combine(gameDir, "standalone");
        Directory.CreateDirectory(dir);
        var st = new StandaloneState(Path.Combine(dir, label + ".state.json"));
        try
        {
            if (File.Exists(st._path))
            {
                using var doc = JsonDocument.Parse(File.ReadAllText(st._path));
                foreach (var e in doc.RootElement.GetProperty("checked").EnumerateArray())
                    st.CheckedInOrder.Add(e.GetInt64());
                st.GoalDone = doc.RootElement.TryGetProperty("goal", out var g) && g.GetBoolean();
            }
        }
        catch (JsonException) { /* a broken state file starts the run over rather than crashing */ }
        catch (KeyNotFoundException) { /* an older file without "checked" is an empty run */ }
        return st;
    }

    public void Save()
    {
        // Written beside itself and moved into place: this runs on every check,
        // and a half-written file would cost the whole run's progress.
        string tmp = _path + ".tmp";
        File.WriteAllText(tmp, JsonSerializer.Serialize(new
        {
            @checked = CheckedInOrder,
            goal = GoalDone,
        }));
        File.Move(tmp, _path, overwrite: true);
    }
}
