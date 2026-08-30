// Does pressing AP Play actually put the player in the map?
//
// ⚠⚠ WHAT THIS CAUGHT
//
// The launcher refused the seed before ever sending slot_data, on the grounds
// that the game had no NewGRFs loaded. But the game cannot have them loaded at
// that moment: the mod fills _grfconfig_newgame from slot_data itself, in
// AP_ConsumeWorldStart, and only then generates the world. The check ran
// before the thing it was checking for could possibly have happened, so it
// blocked the map every single time.
//
// This drives the real game over the real pipe and watches what it does.
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using LauncherV2.Plugins.OpenTTD;

int pass = 0, fail = 0;
void Check(string what, bool ok, string detail = "")
{
    Console.WriteLine($"  {(ok ? "ok  " : "FAIL")} {what}{(detail.Length > 0 ? "  " + detail : "")}");
    if (ok) pass++; else fail++;
}

string exe = args.Length > 0 ? args[0]
    : @"C:\spil\Multiworld Launcher\Games\OpenTTD\openttd.exe";
if (!File.Exists(exe)) { Console.WriteLine($"  SKIPPED: no game at {exe}"); return 0; }
string gameDir = Path.GetDirectoryName(exe)!;

// The seed's own words, in the shape archipelago.cpp reads them. Marco's YAML:
// SHARK, Hover and Vactrain on; 2048x2048 (2^11); 1950.
string slotJson = """
{
  "start_year": 1950, "map_x": 11, "map_y": 11,
  "enable_shark_ships": 1, "enable_hover_vehicles": 1, "enable_vactrain": 1,
  "enable_iron_horse": 0, "enable_military_items": 0, "enable_heqs": 0,
  "enable_aircraftpack": 0, "enable_firs": 0,
  "win_difficulty": 4, "landscape": 0,
  "required_newgrf": [
    { "grfid": "4a44bbb1", "name": "SHARK",          "min_version": 0 },
    { "grfid": "444a5901", "name": "Vactrain Set",   "min_version": 0 },
    { "grfid": "485a0101", "name": "Hover Vehicles", "min_version": 0 }
  ]
}
""";

string pipeName = $"worldproof_{Environment.ProcessId}";
var pipe = new OpenTTDPipeServer(pipeName);

var log = new List<string>();
IReadOnlyList<OpenTTDPipeServer.LoadedGrf>? firstList = null;
using var listSeen = new SemaphoreSlim(0);

pipe.LogReported += t => { lock (log) log.Add(t); };
pipe.GrfListReceived += l => { firstList ??= l; listSeen.Release(); };

using var game = Process.Start(new ProcessStartInfo
{
    FileName = exe,
    Arguments = $"-ap-pipe {pipeName}",
    WorkingDirectory = gameDir,
    UseShellExecute = false,
});
Check("the game starts", game != null);

using var cts = new CancellationTokenSource(TimeSpan.FromMinutes(4));
try
{
    await pipe.WaitForGameAsync(cts.Token);
    _ = pipe.RunAsync(cts.Token);
    Check("it connects to the launcher", true);

    bool got = await listSeen.WaitAsync(TimeSpan.FromSeconds(45), cts.Token);
    Check("it reports its NewGRF list at the title screen", got,
          $"{firstList?.Count ?? 0} loaded — expected 0, the seed has not been sent yet");

    // This is the step the old gate never reached.
    // ⚠ A synthetic slot_data is not a session. Without locked_vehicles and
    // item_id_to_name the mod drops into legacy mode and never reaches
    // SessionStart, so nothing downstream of it can be measured. Pass a real
    // one -- dumped from a generated seed -- as the second argument.
    string realPath = args.Length > 1 ? args[1] : "";
    using var doc = JsonDocument.Parse(
        realPath.Length > 0 && File.Exists(realPath)
            ? File.ReadAllText(realPath) : slotJson);
    Console.WriteLine(realPath.Length > 0 && File.Exists(realPath)
        ? $"  --   using the real slot_data from {Path.GetFileName(realPath)}"
        : "  --   using the synthetic slot_data");
    await pipe.SendSeedAsync("WorldStartProof");
    await pipe.SendSlotDataAsync(doc.RootElement);
    Check("the seed and slot_data are sent", true);

    // World generation on a 2048x2048 map takes a while.
    string Joined() { lock (log) return string.Join("\n", log); }
    bool Saw(params string[] any)
        => any.Any(s => Joined().Contains(s, StringComparison.OrdinalIgnoreCase));

    bool generating = false, inWorld = false;
    for (int i = 0; i < 200 && !inWorld; i++)
    {
        await Task.Delay(1000, cts.Token);
        generating |= Saw("generating world", "scheduling world generation",
                          "auto-start");
        // ⚠ Matched on what the game ACTUALLY prints. A first version looked
        // for "scheduling world generation" and reported failure while the log
        // right beside it said "Auto-start: generating world".
        inWorld |= Saw("world started", "world ready", "generation complete",
                       "in game", "GM_NORMAL", "company", "playing");
    }

    Check("the mod schedules world generation from the seed", generating);
    Check("and the game reaches the map", inWorld);

    Console.WriteLine("\n  --- what the game said ---");
    lock (log)
        foreach (string l in log) Console.WriteLine("    " + l);
}
catch (OperationCanceledException)
{
    Check("the run finishes inside four minutes", false, "timed out");
}
finally
{
    try { if (game is { HasExited: false }) game.Kill(true); } catch { }
}

Console.WriteLine($"\n{pass} ok, {fail} fejl");
return fail == 0 ? 0 : 1;
