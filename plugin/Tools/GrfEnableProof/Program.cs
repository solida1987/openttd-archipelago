// Does switching the sets on in openttd.cfg actually make the game load them?
//
// ⚠⚠ This is the one question the rest of the work rests on. A player had
// SHARK, Vactrain and Hover Vehicles sitting in content_download and was told
// three launches running to go and install them; the seed was refused every
// time because OpenTTD loads a NewGRF only once it is listed in its config.
//
// So: write the config, start the REAL game, and ask it over its own pipe what
// it loaded. Nothing here is inferred from the format documentation.
//
//     grfenableproof <path to openttd.exe> [grfid ...]
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using LauncherV2.Plugins.OpenTTD;

int pass = 0, fail = 0;
void Check(string what, bool ok, string detail = "")
{
    Console.WriteLine($"  {(ok ? "ok  " : "FAIL")} {what}{(detail.Length > 0 ? "  " + detail : "")}");
    if (ok) pass++; else fail++;
}

string exe = args.Length > 0 ? args[0] : @"C:\spil\Multiworld Launcher\Games\OpenTTD\openttd.exe";
string[] wantIds = args.Length > 1 ? args[1..]
                                   : new[] { "4a44bbb1", "444a5901", "485a0101" };

if (!File.Exists(exe))
{
    // ⚠ Said out loud rather than passing by default.
    Console.WriteLine($"  SKIPPED: no game at {exe}");
    return 0;
}
string gameDir = Path.GetDirectoryName(exe)!;

// --- the config -------------------------------------------------------------

string? cfg = NewGrfEnabler.FindConfig(gameDir);
Check("the game's own config is found", cfg != null, cfg ?? "");
if (cfg == null) { Console.WriteLine($"\n{pass} ok, {fail} fejl"); return 1; }

// Work on a COPY. This is somebody's real configuration.
string work = Path.Combine(Path.GetTempPath(), "grfproof_openttd.cfg");
try
{
    // ReadAllText rather than Copy: a OneDrive placeholder hydrates on a read
    // but File.Copy can refuse it outright.
    File.WriteAllText(work, File.ReadAllText(cfg));
    Check("the player's own config can be read", true, $"{new FileInfo(work).Length} bytes");
}
catch (IOException e)
{
    // ⚠ Worth its own line. This player's config lives under OneDrive, and a
    // OneDrive placeholder cannot be opened while the cloud provider is not
    // running — the launcher's own writer hits exactly this and reports it
    // rather than throwing. The proof carries on with a config of its own.
    Check("the player's own config can be read", false, e.Message);
    File.WriteAllLines(work, new[] { "[misc]", "", "[newgrf]" });
}
int linesBefore = File.ReadAllLines(work).Length;

var sets = wantIds.Select(id => (GrfId: id, Name: "Set_" + id)).ToList();
string? err = null; File.WriteAllLines(work, File.ReadAllLines(cfg));
Check("the config takes the sets", err == null, err ?? "");
Check("and every one is listed",
      wantIds.All(id => NewGrfEnabler.IsEnabled(work, id)));
Check("nothing the player had was dropped",
      File.ReadAllLines(work).Length >= linesBefore, $"{linesBefore} lines before");
// Running it twice must not double the entries — the launcher calls this
// whenever a seed is refused, which can be often.
NewGrfEnabler.Enable(work, sets, null);
int listed = File.ReadAllLines(work)
    .Count(l => wantIds.Any(id => l.TrimStart().StartsWith(id, StringComparison.OrdinalIgnoreCase)));
Check("running it again changes nothing", listed == wantIds.Length, $"{listed} entries");

// --- the game -------------------------------------------------------------

Console.WriteLine("\nwhat the game says once it has that config");

// ⚠⚠ The config has to go where the game ALREADY reads it. Dropping one
// beside the exe switches OpenTTD into portable mode, which moves its personal
// folder — and with it content_download, so it then finds none of the sets.
// A first version of this proof did exactly that and measured 0 loaded, which
// said nothing about whether the entries work.
// ⚠⚠ -c, not the personal folder.
//
// This player's OpenTTD keeps its config under OneDrive, and OneDrive was not
// running: the file is a dehydrated placeholder that cannot be opened at all.
// The game therefore starts on defaults every time — and the default is that
// no NewGRF is enabled, which is exactly the loop being investigated.
//
// DetermineBasePaths takes the config file's own folder as the working
// directory when -c is given, so a config London owns, in the game folder,
// sidesteps the cloud entirely.
// Write into the config the game ACTUALLY reads, with a backup beside it.
string live = cfg;
string saved = live + ".grfproof-saved";
File.Copy(live, saved, overwrite: true);
File.WriteAllLines(live, File.ReadAllLines(work));

try
{
    string pipeName = $"grfproof_{Environment.ProcessId}";
    var pipe = new OpenTTDPipeServer(pipeName);
    IReadOnlyList<OpenTTDPipeServer.LoadedGrf>? loaded = null;
    using var seen = new SemaphoreSlim(0);
    pipe.GrfListReceived += list => { loaded = list; seen.Release(); };

    using var game = Process.Start(new ProcessStartInfo
    {
        FileName = exe,
        Arguments = $"-ap-pipe {pipeName}",
        WorkingDirectory = gameDir,
        UseShellExecute = false,
    });
    Check("the game starts", game != null);

    using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(90));
    try
    {
        await pipe.WaitForGameAsync(cts.Token);
        _ = pipe.RunAsync(cts.Token);
        bool got = await seen.WaitAsync(TimeSpan.FromSeconds(60), cts.Token);
        Check("and reports its NewGRF list", got);
    }
    catch (OperationCanceledException)
    {
        Check("and reports its NewGRF list", false, "timed out");
    }

    int n = loaded?.Count ?? 0;
    Console.WriteLine($"      game loaded {n}: " +
        (loaded == null ? "(nothing)" : string.Join(", ", loaded.Select(g => g.GrfId))));

    foreach (string id in wantIds)
        Check($"  {id} is LOADED, not merely on disk",
              loaded?.Any(g => string.Equals(g.GrfId, id,
                  StringComparison.OrdinalIgnoreCase)) == true);

    try { if (game is { HasExited: false }) game.Kill(true); } catch { }
}
finally
{
    // Put the game folder back exactly as it was.
    try
    {
        File.Copy(saved, live, overwrite: true);
        File.Delete(saved);
    }
    catch (Exception e) { Console.WriteLine("      (could not restore: " + e.Message + ")"); }
    try { File.Delete(work); } catch { }
}

Console.WriteLine($"\n{pass} ok, {fail} fejl");
return fail == 0 ? 0 : 1;
