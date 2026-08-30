using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using LauncherV2.Core;

namespace LauncherV2.Plugins.OpenTTD;

///
/// OpenTTD with Archipelago.
///
/// The game holds no Archipelago connection of its own. The launcher connects,
/// starts a named pipe, and hands the game a pipe name on the command line;
/// everything after that is translation. See docs/ap_pipe_protocol.md in the
/// game's repository.
///
/// That split is what makes the NewGRF guard possible: the seed's requirements
/// arrive here, the game reports what it has loaded, and play does not start
/// until the two agree. Neither side could answer that question alone.
///
public sealed class OpenTTDPlugin : IGamePlugin
{
    // --- Identity ---

    public string GameId      => "openttd_archipelago";
    public string DisplayName => "OpenTTD";
    public string Subtitle    => "Randomizer Mod";
    public string IconPath    => Path.Combine(AppContext.BaseDirectory, "Assets", "openttd_archipelago.png");
    public string ApWorldName => "OpenTTD";

    public string Description =>
        "Build a transport empire while Archipelago holds your vehicles hostage. " +
        "Missions, ruins and stars across the map are the checks; every engine, " +
        "wagon and track type is an item somebody has to find.";

    // No hosted media: nothing is uploaded anywhere on the launcher's behalf.
    public string? VideoPreviewUrl => null;
    public string[] ScreenshotUrls => Array.Empty<string>();

    public string   ThemeAccentColor => "#3D8B37";
    public string[] GameBadges       => new[] { "Simulation", "Sandbox", "Open source" };

    // The launcher owns the AP connection for this game -- that is the whole
    // point of the pipe. Without it, nobody can check the NewGRFs before play.
    public bool ConnectsItself    => false;
    public bool SupportsMapTracker => false;   // deliberately out of v1
    public bool IsWebBased        => false;

    // Standalone is out of v1: everything comes from slot_data, so there is nothing to randomise without a server.
    public bool SupportsStandalone => true;

    // --- Install state ---

    public string? InstalledVersion { get; private set; }
    public string? AvailableVersion { get; private set; }
    // Presence on disk decides, not a property someone remembered to set.
    public bool    IsInstalled      => File.Exists(ExePath);
    public bool    IsRunning        { get; private set; }

    public string GameDirectory =>
        Path.Combine(AppContext.BaseDirectory, "Games", "OpenTTD");

    private string ExePath => Path.Combine(GameDirectory, "openttd.exe");

    // --- Events ---

    public event Action<long[]>? LocationsChecked;
    public event Action<int>?    GameExited;
    public event Action?         GoalCompleted;

    // Never raised here — declared because interface events cannot carry
    // defaults in C#.
    public event Action<long[]>? LocationsMissing;
    public event Action<string>? LogLine;

    // Raised once per standalone check, as "<location>: <item>".
    public event Action<string>? StandaloneItemReceived;

    // --- Session state ---

    private OpenTTDPipeServer? _pipe;
    private Process?           _game;
    private CancellationTokenSource? _sessionCts;

    private IReadOnlyDictionary<string, long> _nameToId =
        new Dictionary<string, long>(StringComparer.Ordinal);
    private IReadOnlyDictionary<long, string> _idToLabel =
        new Dictionary<long, string>();
    private IReadOnlyDictionary<long, string> _idToName =
        new Dictionary<long, string>();

    private JsonElement? _slotData;
    private ApConnectionState _apState = ApConnectionState.Disconnected;

    // The GRF handshake happens once per pipe, but slot_data, the location
    // table and the checked-list arrive whenever the AP session has them --
    // often AFTER the handshake. These latches let whichever side is late
    // push its part the moment it lands.
    private volatile bool _accepted;
    /// The game asked for shop hints; retry once the location table lands.
    private volatile bool _scoutWanted;
    // ⚠⚠ ONE GATE, CLAIMED ATOMICALLY, because two threads race for it: the
    // pipe's GRF handler and OnSlotData on the AP thread. A second SLOTDATA
    // resets the game's mission completion flags mid-session.
    private int _slotDataGate;
    private readonly HashSet<long> _checkedIds = new();

    // The standalone seed THIS run picked. Without it a random pick was
    // forgotten and the tracker described the first seed on disk instead.
    private string? _standaloneLabel;

    // Scout replies, by location id. Kept so a second SCOUT: is answered from
    // memory rather than by asking the server again.
    private readonly Dictionary<long, string> _scouted = new();

    // --- Update / install ---

    private const string GithubRepo = "solida1987/openttd-archipelago";

    private static readonly System.Net.Http.HttpClient _http = CreateHttp();
    private static System.Net.Http.HttpClient CreateHttp()
    {
        var h = new System.Net.Http.HttpClient();
        h.DefaultRequestHeaders.UserAgent.TryParseAdd("Multiworld-Launcher/3");
        return h;
    }

    private string VersionStampPath => Path.Combine(GameDirectory, "ap_version.txt");

    public async Task CheckForUpdateAsync(CancellationToken ct = default)
    {
        try
        {
            string json = await _http.GetStringAsync(
                $"https://api.github.com/repos/{GithubRepo}/releases/latest", ct);
            using var doc = JsonDocument.Parse(json);
            AvailableVersion = doc.RootElement.GetProperty("tag_name").GetString();
            InstalledVersion = File.Exists(VersionStampPath)
                ? File.ReadAllText(VersionStampPath).Trim() : null;
        }
        // Offline is not an error state, but a version left over from an
        // earlier check would advertise an update this run cannot fetch --
        // IGamePlugin asks for null instead.
        catch (System.Net.Http.HttpRequestException) { AvailableVersion = null; }
        catch (TaskCanceledException)                { AvailableVersion = null; }
        catch (JsonException)                        { AvailableVersion = null; }
        catch (KeyNotFoundException)                 { AvailableVersion = null; }
    }

    public async Task InstallOrUpdateAsync(IProgress<(int Pct, string Msg)> progress,
                                           CancellationToken ct = default)
    {
        progress.Report((5, "Looking up the latest release..."));
        string json = await _http.GetStringAsync(
            $"https://api.github.com/repos/{GithubRepo}/releases/latest", ct);
        using var doc = JsonDocument.Parse(json);
        string tag = doc.RootElement.GetProperty("tag_name").GetString() ?? "unknown";

        string? url = null;
        foreach (var a in doc.RootElement.GetProperty("assets").EnumerateArray())
        {
            string name = a.GetProperty("name").GetString() ?? "";
            if (name == "game_package.zip" ||
                (name.StartsWith("openttd-archipelago-") && name.EndsWith("-win64.zip")))
            {
                url = a.GetProperty("browser_download_url").GetString();
                if (name == "game_package.zip") break;
            }
        }
        if (url == null)
            throw new InvalidOperationException(
                "The latest release has no game package. Try again later.");

        string tmp = Path.Combine(Path.GetTempPath(), $"ottd_ap_{Guid.NewGuid():N}.zip");
        try
        {
            progress.Report((15, $"Downloading {tag}..."));
            using (var resp = await _http.GetAsync(url,
                       System.Net.Http.HttpCompletionOption.ResponseHeadersRead, ct))
            {
                resp.EnsureSuccessStatusCode();
                long? total = resp.Content.Headers.ContentLength;
                await using var src = await resp.Content.ReadAsStreamAsync(ct);
                await using var dst = File.Create(tmp);
                var buf = new byte[1 << 16];
                long done = 0; int read;
                while ((read = await src.ReadAsync(buf, ct)) > 0)
                {
                    await dst.WriteAsync(buf.AsMemory(0, read), ct);
                    done += read;
                    if (total is > 0)
                        progress.Report((15 + (int)(60.0 * done / total.Value),
                            $"Downloading {tag} ({done / 1048576}MB / {total / 1048576}MB)..."));
                }
            }

            progress.Report((80, "Installing..."));
            Directory.CreateDirectory(GameDirectory);
            ExtractPreservingUserData(tmp, GameDirectory);

            File.WriteAllText(VersionStampPath, tag);
            InstalledVersion = tag;
            progress.Report((100, $"OpenTTD Archipelago {tag} installed."));
        }
        finally
        {
            try { File.Delete(tmp); } catch (IOException) { }
        }
    }

    // The player's world lives in the game folder: saves, their openttd.cfg,
    // their NewGRFs, standalone progress. An update must never touch those.
    private static void ExtractPreservingUserData(string zipPath, string destDir)
    {
        using var zip = System.IO.Compression.ZipFile.OpenRead(zipPath);

        // Releases may wrap everything in one root folder; strip it.
        string prefix = "";
        var first = zip.Entries.FirstOrDefault();
        if (first != null)
        {
            int slash = first.FullName.IndexOf('/');
            if (slash > 0)
            {
                string candidate = first.FullName[..(slash + 1)];
                if (zip.Entries.All(e => e.FullName.StartsWith(candidate) || e.FullName == candidate))
                    prefix = candidate;
            }
        }

        foreach (var entry in zip.Entries)
        {
            string rel = entry.FullName[prefix.Length..].Replace('/', Path.DirectorySeparatorChar);
            if (rel.Length == 0 || rel.EndsWith(Path.DirectorySeparatorChar)) continue;

            bool userData =
                rel.StartsWith("save" + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) ||
                rel.StartsWith("standalone" + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) ||
                rel.StartsWith("newgrf" + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) ||
                // Sets the game downloaded for itself: its own folder, and one
                // an update has no business replacing.
                rel.StartsWith(Path.Combine("data", "content_download") + Path.DirectorySeparatorChar,
                               StringComparison.OrdinalIgnoreCase) ||
                rel.Equals(Path.Combine("data", "openttd.cfg"), StringComparison.OrdinalIgnoreCase) ||
                // A portable install keeps its config beside the exe, and
                // NewGrfEnabler.FindConfig honours that -- so it is the
                // player's file just as much as the one under data\.
                rel.Equals("openttd.cfg", StringComparison.OrdinalIgnoreCase);

            string target = Path.Combine(destDir, rel);
            if (userData && File.Exists(target)) continue;

            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            System.IO.Compression.ZipFileExtensions.ExtractToFile(entry, target, overwrite: true);
        }
    }

    public Task<bool> VerifyInstallAsync(CancellationToken ct = default)
        => Task.FromResult(File.Exists(ExePath) &&
                           Directory.Exists(Path.Combine(GameDirectory, "baseset")));

    public string? ValidateExistingInstall(string folder)
    {
        if (!File.Exists(Path.Combine(folder, "openttd.exe")))
            return "No openttd.exe in that folder.";
        // The stock game has no pipe client, so it would start and then sit
        // there with nothing connected -- a confusing way to find out.
        if (!Directory.Exists(Path.Combine(folder, "baseset")))
            return "That folder has openttd.exe but no baseset/ -- it does not look like a complete OpenTTD.";
        return null;
    }

    // --- Launch ---

    public async Task LaunchAsync(ApSession session, CancellationToken ct = default)
    {
        if (!File.Exists(ExePath))
            throw new InvalidOperationException("OpenTTD is not installed. Set the game folder first.");

        await StopAsync();
        _sessionCts = new CancellationTokenSource();

        // Session state must not leak between launches. slot_data is NOT
        // cleared: the join flow pushes it before LaunchAsync runs, and each
        // session overwrites it -- wiping it here starved the game of its
        // world. Checked ids and hint labels are seed-specific and must go.
        _idToLabel = new Dictionary<long, string>();
        _accepted = false;
        _scoutWanted = false;
        Interlocked.Exchange(ref _slotDataGate, 0);
        lock (_checkedIds) _checkedIds.Clear();
        lock (_scouted) _scouted.Clear();

        // A per-launch suffix: a stream left behind by a crashed session can
        // linger, and colliding with it would hand the new game the old pipe.
        string pipeName = $"openttd_ap_{Environment.ProcessId}_{Guid.NewGuid().ToString("N")[..8]}";
        Log($"--- launch: slot {session.SlotName} on {session.ServerUri}, pipe {pipeName} ---");
        var pipe = new OpenTTDPipeServer(pipeName);
        _pipe = pipe;
        pipe.SetLocationTable(_nameToId);

        WireEvents(pipe);

        // The pipe must exist before the game starts: the game connects on its
        // first tick and gives up if nothing is listening.
        using var linked = CancellationTokenSource.CreateLinkedTokenSource(ct, _sessionCts.Token);
        linked.CancelAfter(TimeSpan.FromSeconds(90));

        PrepareNewGrfConfig(_slotData);

        StartGame(pipeName);

        await pipe.WaitForGameAsync(linked.Token);
        _ = pipe.RunAsync(_sessionCts.Token);
    }

    /// Start the game and let it end its OWN session.
    ///
    /// ⚠ The exit handler captures the process it belongs to. Reading the
    /// field instead let a previous run's exit -- which can land long after
    /// the next launch -- report the NEW session as closed.
    private void StartGame(string pipeName)
    {
        var game = Process.Start(new ProcessStartInfo
        {
            FileName         = ExePath,
            Arguments        = $"-ap-pipe {pipeName}",
            WorkingDirectory = GameDirectory,
            UseShellExecute  = false,
        }) ?? throw new InvalidOperationException("Could not start openttd.exe.");

        _game = game;
        IsRunning = true;
        game.EnableRaisingEvents = true;
        game.Exited += (_, _) =>
        {
            if (!ReferenceEquals(_game, game)) return;   // an older run, not this one
            IsRunning = false;
            int code = 0;
            // ObjectDisposedException is an InvalidOperationException; both a
            // disposed handle and one that never started land here.
            try { code = game.ExitCode; } catch (InvalidOperationException) { }
            GameExited?.Invoke(code);
        };
    }

    // Standalone: pick a pre-generated solo seed and answer the game locally.
    // Same pipe, same protocol -- the game cannot tell there is no server.
    public async Task LaunchStandaloneAsync(CancellationToken ct = default)
    {
        if (!File.Exists(ExePath))
            throw new InvalidOperationException("OpenTTD is not installed. Set the game folder first.");

        var labels = StandaloneSeed.ListLabels(GameDirectory);
        if (labels.Count == 0)
            throw new InvalidOperationException(
                "No standalone seeds found. Reinstall the game -- standalone_seeds/ " +
                "ships with it.");

        string? chosen = Core.PluginSettings.Get(GameId, "standalone_seed");
        if (chosen == null || !labels.Contains(chosen))
            chosen = labels[new Random().Next(labels.Count)];
        // Remembered, because the tracker asks again later and a second random
        // pick would describe a seed nobody is playing.
        _standaloneLabel = chosen;

        var seed  = StandaloneSeed.Load(GameDirectory, chosen);
        var state = StandaloneState.Load(GameDirectory, chosen);
        Log($"--- standalone: seed {seed.Label} ({seed.SeedName}), " +
            $"{state.CheckedInOrder.Count}/{seed.Placements.Count} already checked ---");

        await StopAsync();
        _sessionCts = new CancellationTokenSource();

        string pipeName = $"openttd_ap_{Environment.ProcessId}_{Guid.NewGuid().ToString("N")[..8]}";
        var pipe = new OpenTTDPipeServer(pipeName);
        _pipe = pipe;
        pipe.SetLocationTable(seed.NameToId);
        WireStandaloneEvents(pipe, seed, state);

        using var linked = CancellationTokenSource.CreateLinkedTokenSource(ct, _sessionCts.Token);
        linked.CancelAfter(TimeSpan.FromSeconds(90));

        // ⚠ THIS seed's requirements. The field belongs to a JOIN, and reading
        // it here set a standalone world up from whatever multiworld the
        // launcher last connected to.
        PrepareNewGrfConfig(seed.SlotData);

        StartGame(pipeName);

        await pipe.WaitForGameAsync(linked.Token);
        _ = pipe.RunAsync(_sessionCts.Token);
    }

    private void WireStandaloneEvents(OpenTTDPipeServer pipe, StandaloneSeed seed, StandaloneState state)
    {
        var done = new HashSet<long>(state.CheckedInOrder);

        pipe.LogReported += text => Log("game: " + text);

        // ⚠ These two are async void: nothing awaits an event handler, so an
        // exception escaping one takes the launcher down with no log at all.
        // The work lives in named methods so the wrapper stays a wrapper.
        pipe.GrfListReceived += async loaded =>
        {
            try { await StandaloneHandshakeAsync(pipe, seed, state, done, loaded); }
            catch (Exception e) { Log("standalone handshake failed: " + e.Message); }
        };

        pipe.CheckReported += async id =>
        {
            try { await StandaloneCheckAsync(pipe, seed, state, done, id); }
            catch (Exception e) { Log($"standalone check {id} failed: " + e.Message); }
        };

        pipe.GoalReported += () =>
        {
            if (state.GoalDone) return;
            state.GoalDone = true;
            state.Save();
            Log("standalone goal reached");
            GoalCompleted?.Invoke();
        };
    }

    private async Task StandaloneHandshakeAsync(
        OpenTTDPipeServer pipe, StandaloneSeed seed, StandaloneState state,
        HashSet<long> done, IReadOnlyList<OpenTTDPipeServer.LoadedGrf> loaded)
    {
        Log($"game reports {loaded.Count} NewGRF(s): " +
            (loaded.Count == 0 ? "none"
                               : string.Join(", ", loaded.Select(g => $"{g.GrfId} v{g.Version}"))));
        // ⚠⚠ NOT A GATE. This used to refuse the seed here, and that made
        // the whole feature impossible.
        //
        // The mod sets the NewGRFs up ITSELF: when slot_data arrives at the
        // title screen it calls AP_ConsumeWorldStart(), which fills
        // _grfconfig_newgame from the seed and then generates the map
        // (archipelago_manager.cpp, "Slot data ready -> scheduling world
        // generation"). So the list reported BEFORE slot_data is sent is
        // empty by definition -- the seed is what tells the game which sets
        // to load. Refusing on it meant the launcher blocked the exact step
        // that would have loaded them, every single time.
        //
        // What is worth doing is saying so. A set that is genuinely absent
        // from disk is still worth a line in the log; it is not worth
        // refusing over, because the game is the side that can fetch it.
        foreach (var p in GrfProblems(seed.SlotData, loaded)
                             .Where(p => p.State == NewGrfState.Missing))
            Log($"note: {p.Required.DisplayName} is not on disk; the game "
              + "will ask for it when it builds the world.");

        await pipe.SendSeedAsync(seed.SeedName);
        await pipe.SendSlotDataAsync(seed.SlotData);
        await pipe.SendLocationCountAsync(seed.Placements.Count);
        await pipe.SendCheckedAsync(
            done.Where(seed.IdToName.ContainsKey).Select(id => seed.IdToName[id]));

        // Replay what this run already earned; the index dedups.
        int index = 0;
        foreach (long loc in state.CheckedInOrder)
            if (seed.Placements.TryGetValue(loc, out long item))
                await pipe.SendItemAsync(item, index++);

        // ⚠⚠ 2, not 3. Three is the error state in the protocol; only two
        // puts the game's own client into AUTHENTICATED, and a standalone
        // run is as connected as it will ever be.
        await pipe.SendStateAsync(2);
        Log($"standalone accepted; {done.Count} checks replayed");
    }

    private async Task StandaloneCheckAsync(
        OpenTTDPipeServer pipe, StandaloneSeed seed, StandaloneState state,
        HashSet<long> done, long id)
    {
        if (!seed.Placements.TryGetValue(id, out long item) || !done.Add(id)) return;

        state.CheckedInOrder.Add(id);
        state.Save();
        await pipe.SendItemAsync(item, state.CheckedInOrder.Count - 1);

        LocationsChecked?.Invoke(new[] { id });
        string locName  = seed.IdToName.TryGetValue(id, out var ln) ? ln : id.ToString();
        string itemName = seed.ItemNames.TryGetValue(item, out var inm) ? inm : item.ToString();
        StandaloneItemReceived?.Invoke($"{locName}: {itemName}");
    }

    ///
    /// Session log beside the game.
    ///
    /// When a player says "it will not start", the answer is almost always in
    /// what the two sides said to each other -- which NewGRFs the game reported,
    /// whether the seed was refused and why. Without this the only evidence is
    /// a message box the player has already clicked away.
    ///
    private void Log(string line)
    {
        try
        {
            File.AppendAllText(Path.Combine(GameDirectory, "ap_launcher.log"),
                $"{DateTime.Now:HH:mm:ss}  {line}{Environment.NewLine}");
        }
        catch (IOException) { }            // a log that cannot be written is not worth a crash
        catch (UnauthorizedAccessException) { }
    }

    private IApServices? _ap;

    public void OnApServicesAttached(IApServices? services)
    {
        if (_ap != null) _ap.LocationsScouted -= OnLocationsScouted;
        _ap = services;
        if (_ap != null) _ap.LocationsScouted += OnLocationsScouted;
    }

    private void WireEvents(OpenTTDPipeServer pipe)
    {
        pipe.CheckReported += id => LocationsChecked?.Invoke(new[] { id });
        pipe.DeathReported += cause => _ap?.ReportDeath(cause);
        pipe.GoalReported  += () => { Log("goal reached"); GoalCompleted?.Invoke(); };
        pipe.LogReported   += text => Log("game: " + text);

        // ⚠ async void, like the standalone pair: an exception escaping here
        // reaches nobody, so it is caught and written to the session log.
        pipe.GrfListReceived += async loaded =>
        {
            try { await HandshakeAsync(pipe, loaded); }
            catch (Exception e) { Log("handshake failed: " + e.Message); }
        };

        pipe.ScoutRequested += async () =>
        {
            _scoutWanted = true;
            try { await ScoutAsync(pipe); }
            catch (Exception e) { Log("scout failed: " + e.Message); }
        };
    }

    private async Task HandshakeAsync(OpenTTDPipeServer pipe,
                                      IReadOnlyList<OpenTTDPipeServer.LoadedGrf> loaded)
    {
        Log($"game reports {loaded.Count} NewGRF(s): " +
            (loaded.Count == 0 ? "none"
                               : string.Join(", ", loaded.Select(g => $"{g.GrfId} v{g.Version}"))));

        // ⚠⚠ NOT A GATE. This used to refuse the seed here, and that made
        // the whole feature impossible.
        //
        // The mod sets the NewGRFs up ITSELF: when slot_data arrives at the
        // title screen it calls AP_ConsumeWorldStart(), which fills
        // _grfconfig_newgame from the seed and then generates the map
        // (archipelago_manager.cpp, "Slot data ready -> scheduling world
        // generation"). So the list reported BEFORE slot_data is sent is
        // empty by definition -- the seed is what tells the game which sets
        // to load. Refusing on it meant the launcher blocked the exact step
        // that would have loaded them, every single time.
        //
        // What is worth doing is saying so. A set that is genuinely absent
        // from disk is still worth a line in the log; it is not worth
        // refusing over, because the game is the side that can fetch it.
        if (_slotData.HasValue)
            foreach (var p in GrfProblems(_slotData.Value, loaded)
                                 .Where(p => p.State == NewGrfState.Missing))
                Log($"note: {p.Required.DisplayName} is not on disk; the game "
                  + "will ask for it when it builds the world.");

        Log(_slotData.HasValue
            ? $"accepted; sending slot_data and {SeedLocationCount()} locations"
            : "accepted, but no slot_data has arrived yet");

        _accepted = true;
        // Before slot_data: the game decides start-vs-continue the moment
        // slot_data lands, and the seed name is the savegame key.
        if (_ap?.SeedName is { Length: > 0 } seedName)
            await pipe.SendSeedAsync(seedName);
        // ⚠ OnSlotData runs on the AP thread and can reach this same gate
        // between _accepted going true and the line below. Whoever claims it
        // sends; the other stays quiet.
        if (_slotData.HasValue && Interlocked.CompareExchange(ref _slotDataGate, 1, 0) == 0)
            await pipe.SendSlotDataAsync(_slotData.Value);
        if (SeedLocationCount() > 0) await pipe.SendLocationCountAsync(SeedLocationCount());
        await PushCheckedAsync(pipe);
        await pipe.SendStateAsync(StateNumber(_apState));

        // Replay the item stream from index 0. Items delivered while the
        // game was still starting had no pipe to land in; the index lets
        // the game recognise what it already handled.
        if (_ap != null) await _ap.ResyncAsync();
    }

    /// The shop slots, as the apworld names them (locations.py). Only these
    /// are scouted: they are the ones the game draws a label on, and scouting
    /// the whole seed would spoil it for the player who asked for one window.
    private const string ShopLocationPrefix = "Shop_Purchase_";

    ///
    /// The game asked what its shop slots hold.
    ///
    /// Two halves: whatever labels are already known go down the pipe now, and
    /// the rest is asked of the server. The reply arrives on
    /// <see cref="OnLocationsScouted"/>, which sends the missing HINT lines --
    /// the game keeps its list keyed by name and takes them whenever they land.
    ///
    private async Task ScoutAsync(OpenTTDPipeServer pipe)
    {
        var known = new List<(string Name, string Label)>();
        var labelled = new HashSet<long>();

        foreach (var (id, label) in _idToLabel)
            if (_idToName.TryGetValue(id, out string? n) && labelled.Add(id))
                known.Add((n, label));
        lock (_scouted)
            foreach (var (id, label) in _scouted)
                if (_idToName.TryGetValue(id, out string? n) && labelled.Add(id))
                    known.Add((n, label));

        await SendHintsAsync(pipe, known);

        long[] ask = (_ap?.UncheckedLocations() ?? Array.Empty<long>())
            .Where(id => !labelled.Contains(id)
                         && _idToName.TryGetValue(id, out string? n)
                         && n.StartsWith(ShopLocationPrefix, StringComparison.Ordinal))
            .ToArray();
        if (ask.Length == 0 || _ap == null) return;

        Log($"scouting {ask.Length} shop location(s)");
        await _ap.ScoutLocationsAsync(ask);
    }

    /// A scout reply. The label is the player the item belongs to; the item's
    /// own name is another game's datapackage, which this plugin never holds.
    private void OnLocationsScouted(ApNetworkItem[] items)
    {
        var send = new List<(string Name, string Label)>();
        lock (_scouted)
            foreach (var it in items)
            {
                string label = _ap?.ResolvePlayerName(it.Player) ?? $"Player {it.Player}";
                _scouted[it.LocationId] = label;
                if (_idToName.TryGetValue(it.LocationId, out string? name))
                    send.Add((name, label));
            }

        var pipe = _pipe;
        if (pipe != null && send.Count > 0) _ = SendHintsAsync(pipe, send);
    }

    private async Task SendHintsAsync(OpenTTDPipeServer pipe,
                                      IReadOnlyList<(string Name, string Label)> hints)
    {
        try
        {
            foreach (var (name, label) in hints) await pipe.SendHintAsync(name, label);
        }
        catch (Exception e) { Log("could not send hints: " + e.Message); }
    }

    ///
    /// Does the player have what this seed was built from?
    ///
    /// The list comes from the game rather than from a folder scan on purpose:
    /// a file on disk that the player has not enabled is not loaded, and the
    /// game is the only side that knows the difference.
    ///
    /// null when everything needed is loaded, otherwise what to tell them.
    private string? EvaluateGrfs(JsonElement slotData,
                                        IReadOnlyList<OpenTTDPipeServer.LoadedGrf> loaded)
        => NewGrfRequirements.Explain(GrfProblems(slotData, loaded));

    /// The unsatisfied requirements themselves, not just the sentence about
    /// them — the offer needs the ids to ask the content service for.
    private IReadOnlyList<NewGrfCheckResult> GrfProblems(
        JsonElement slotData, IReadOnlyList<OpenTTDPipeServer.LoadedGrf> loaded)
    {
        var required = NewGrfRequirements.FromSlotData(slotData);
        if (required.Count == 0) return Array.Empty<NewGrfCheckResult>();

        // The scanner's shape, filled from what the game said. Path and name
        // stay empty: the launcher never saw these as files.
        var installed = loaded.Select(g => new NewGrfInfo(
            Path: "", GrfId: g.GrfId, Name: null, Description: null,
            Version: g.Version, MinVersion: null, Url: null, Error: null));

        // ⚠ The disk as well as the game. They answer different questions —
        // "do you have the file" and "did you tick it" — and the launcher used
        // to ask only the second while phrasing the answer as the first.
        return NewGrfRequirements.Evaluate(installed, required, ScanInstalledSets())
                                 .Where(r => !r.Ok).ToList();
    }

    ///
    /// Offer to fetch what is missing, then refuse either way.
    ///
    /// The refusal stands even when the download runs: OpenTTD reads NewGRFs
    /// at startup, so the sets cannot join a session already in progress. What
    /// changes is what the player does next — restart with the content in
    /// hand, instead of going hunting for it.
    ///
    private async Task<string> OfferGrfFetchAsync(
        OpenTTDPipeServer pipe, IReadOnlyList<NewGrfCheckResult> problems,
        string refusal, Action<string> log)
    {
        // ⚠⚠ FIRST: the sets the player already has. Downloading them again
        // achieves nothing — OpenTTD loads a NewGRF only once it is listed in
        // its config, and a player with all three in content_download went
        // round this loop three times being told to install what was already
        // installed.
        var offDisk = problems.Where(p => p.State == NewGrfState.NotEnabled).ToList();
        if (offDisk.Count > 0)
        {
            string? cfg = NewGrfEnabler.FindConfig(GameDirectory);
            if (cfg == null)
            {
                log("no openttd.cfg found; cannot enable sets automatically");
            }
            else if (NewGrfOfferDialog.Ask(offDisk))
            {
                string? err = NewGrfEnabler.Enable(cfg,
                    offDisk.Select(p => (p.Required.GrfId, p.Required.DisplayName)), log);
                if (err != null)
                {
                    log("could not enable: " + err);
                    return refusal + " " + err;
                }
                // NewGRFs are read at startup, so this run cannot use them —
                // but the next one can, with nothing left for the player to do.
                return "Those sets are now switched on in OpenTTD. Close the "
                     + "game and press AP Play again — nothing needs downloading.";
            }
            else log("player declined to enable the sets");
        }

        // Too old is a different job: the service would hand back the same
        // package, and replacing a set the player already chose is their call.
        var fetchable = problems.Where(p => p.State == NewGrfState.Missing).ToList();
        if (fetchable.Count == 0) return refusal;

        if (!NewGrfOfferDialog.Ask(fetchable))
        {
            log("player declined the NewGRF download");
            return refusal;
        }

        log($"fetching {fetchable.Count} NewGRF set(s) from OpenTTD's content service");
        await pipe.SendGrfFetchAsync(fetchable.Select(p => p.Required.GrfId));
        // The content list streams in; give it a moment before asking for the
        // download, so the lookup has something to have found.
        await Task.Delay(TimeSpan.FromSeconds(5));
        await pipe.SendGrfDownloadAsync();

        return refusal + " The launcher has asked OpenTTD to download the "
             + "missing sets — restart the game, enable them, and reconnect.";
    }

    // --- AP to game ---

    public async Task ReceiveItemsAsync(ApNetworkItem[] items, int index,
                                        CancellationToken ct = default)
    {
        var pipe = _pipe;
        if (pipe == null) return;
        for (int i = 0; i < items.Length; i++)
            await pipe.SendItemAsync(items[i].ItemId, index + i);
    }

    public void OnApStateChanged(ApConnectionState state)
    {
        _apState = state;
        _ = _pipe?.SendStateAsync(StateNumber(state));
    }

    /// How many locations THIS SEED holds — the "y" in the game's "x of y".
    ///
    /// ⚠⚠ NOT _idToName.Count. That is the DATAPACKAGE: every location name
    /// the OpenTTD world could ever hand out (200 missions + 600 shop +
    /// ruins + 10 demigods + 1000 stars + the goal). It is the same number
    /// for every OpenTTD seed ever generated, so the game showed "2 / 1911"
    /// to a player whose seed had 355 locations — and he reasonably read that
    /// as the seed being nearly empty.
    ///
    /// The slot's own two lists ARE the seed. They are disjoint and their
    /// union is complete: ApJoinSession seeds the checked set from the
    /// server's ConnectedChecked at connect, and UncheckedLocations() is
    /// ConnectedMissing minus that set. London's own Join card has always
    /// used the same sum.
    ///
    /// Falls back to the datapackage only when there is no AP session at all
    /// (offline standalone seeds take the seed.Placements path instead).
    private int SeedLocationCount()
    {
        int n = (_ap?.CheckedLocations().Length ?? 0)
              + (_ap?.UncheckedLocations().Length ?? 0);
        return n > 0 ? n : _idToName.Count;
    }

    public void OnLocationTable(IReadOnlyDictionary<string, long> nameToId)
    {
        _nameToId = nameToId;
        _idToName = nameToId.GroupBy(kv => kv.Value)
                            .ToDictionary(g => g.Key, g => g.First().Key);
        _pipe?.SetLocationTable(nameToId);

        // The table usually lands after the GRF handshake -- the datapackage
        // is a second round trip. Push the counter and any parked checked
        // names that could not be resolved without it.
        var pipe = _pipe;
        if (pipe != null && _accepted)
        {
            _ = pipe.SendLocationCountAsync(SeedLocationCount());
            _ = PushCheckedAsync(pipe);

            // A scout asked for before this table arrived had no names to work
            // with and quietly did nothing. Now it can be answered.
            if (_scoutWanted)
            {
                _ = Task.Run(async () =>
                {
                    try { await ScoutAsync(pipe); }
                    catch (Exception e) { Log("scout retry failed: " + e.Message); }
                });
            }
        }
    }

    /// Locations the server says are already checked -- resume sync. Parked
    /// until the pipe is up and the table can turn the ids into names.
    public void OnCheckedLocations(long[] locationIds)
    {
        lock (_checkedIds)
            foreach (long id in locationIds) _checkedIds.Add(id);
        var pipe = _pipe;
        if (pipe != null && _accepted) _ = PushCheckedAsync(pipe);
    }

    private async Task PushCheckedAsync(OpenTTDPipeServer pipe)
    {
        List<string> names;
        lock (_checkedIds)
            names = _checkedIds.Where(_idToName.ContainsKey)
                               .Select(id => _idToName[id]).ToList();
        if (names.Count > 0) await pipe.SendCheckedAsync(names);
    }

    public void OnLocationHints(IReadOnlyDictionary<long, string> idToLabel) => _idToLabel = idToLabel;

    ///
    /// The seed's own settings. Cloned because the launcher does not promise
    /// the element outlives the call, and this is read again every time the
    /// game reconnects.
    ///
    public void OnSlotData(JsonElement slotData)
    {
        _slotData = slotData.Clone();
        // Arrived after the handshake: send it now, once. A second SLOTDATA
        // mid-game would reset the game's mission completion flags -- and the
        // handshake races this call, so the gate is claimed, not read.
        var pipe = _pipe;
        if (pipe != null && _accepted &&
            Interlocked.CompareExchange(ref _slotDataGate, 1, 0) == 0)
            _ = pipe.SendSlotDataAsync(_slotData.Value);
    }

    private static int StateNumber(ApConnectionState s) => s switch
    {
        ApConnectionState.Disconnected => 0,
        ApConnectionState.Connecting   => 1,
        ApConnectionState.Connected    => 2,
        _                              => 3,
    };

    // --- Teardown ---

    public async Task StopAsync()
    {
        try { _sessionCts?.Cancel(); } catch (ObjectDisposedException) { }

        if (_pipe != null)
        {
            await _pipe.DisposeAsync();
            _pipe = null;
        }

        var game = _game;
        _game = null;
        if (game != null)
        {
            try
            {
                if (!game.HasExited)
                {
                    game.CloseMainWindow();
                    // Wait for it to actually go. The next launch rewrites
                    // openttd.cfg and reuses the game folder, and returning
                    // while the old process still holds both is what made a
                    // relaunch look like it had ignored the new seed.
                    using var wait = new CancellationTokenSource(TimeSpan.FromSeconds(5));
                    try { await game.WaitForExitAsync(wait.Token); }
                    catch (OperationCanceledException) { }
                }
            }
            catch (InvalidOperationException) { }
            game.Dispose();
        }

        _sessionCts?.Dispose();
        _sessionCts = null;
        IsRunning = false;
    }

    // --- UI ---

    public UIElement? CreateSettingsPanel()
    {
        var panel = new System.Windows.Controls.StackPanel
        {
            Margin = new System.Windows.Thickness(12),
        };
        panel.Children.Add(new System.Windows.Controls.TextBlock
        {
            Text = "Joining a multiworld means handing the host a YAML. This builds "
                 + "one from the apworld's own option list, so every key in it is a "
                 + "key the generator accepts.",
            TextWrapping = System.Windows.TextWrapping.Wrap,
            Margin = new System.Windows.Thickness(0, 0, 0, 10),
        });
        var button = new System.Windows.Controls.Button
        {
            Content = "Create an Archipelago YAML…",
            Padding = new System.Windows.Thickness(16, 6, 16, 6),
            HorizontalAlignment = System.Windows.HorizontalAlignment.Left,
        };
        button.Click += (_, _) => OpenTTDYamlDialog.ShowFor(System.Windows.Window.GetWindow(panel));
        panel.Children.Add(button);

        // Standalone seed choice. Every entry is a real pre-generated solo
        // seed; "Random" picks one at launch.
        panel.Children.Add(new System.Windows.Controls.TextBlock
        {
            Text = "Standalone seed",
            FontWeight = System.Windows.FontWeights.Bold,
            Margin = new System.Windows.Thickness(0, 16, 0, 4),
        });
        var combo = new System.Windows.Controls.ComboBox
        {
            MinWidth = 220,
            HorizontalAlignment = System.Windows.HorizontalAlignment.Left,
        };
        combo.Items.Add("Random");
        foreach (string label in StandaloneSeed.ListLabels(GameDirectory))
            combo.Items.Add(label);
        string current = Core.PluginSettings.Get(GameId, "standalone_seed") ?? "Random";
        combo.SelectedItem = combo.Items.Contains(current) ? current : "Random";
        combo.SelectionChanged += (_, _) =>
        {
            string? sel = combo.SelectedItem as string;
            Core.PluginSettings.Set(GameId, "standalone_seed",
                sel == "Random" ? null : sel);
        };
        panel.Children.Add(combo);
        panel.Children.Add(new System.Windows.Controls.TextBlock
        {
            Text = "Progress is kept per seed, so you can leave and continue later.",
            FontSize = 11,
            Opacity = 0.7,
            Margin = new System.Windows.Thickness(0, 4, 0, 0),
        });

        return panel;
    }

    /// The seed the tracker should describe.
    ///
    /// ⚠ A pinned seed wins, then the one this run actually launched. Falling
    /// straight through to labels[0] made the tracker describe a different
    /// seed from the one being played every time the pick was random.
    private string CurrentStandaloneLabel(IReadOnlyList<string> labels)
    {
        string? pinned = Core.PluginSettings.Get(GameId, "standalone_seed");
        if (pinned != null && labels.Contains(pinned)) return pinned;
        if (_standaloneLabel != null && labels.Contains(_standaloneLabel)) return _standaloneLabel;
        return labels[0];
    }

    // The tracker's world: in standalone the chosen seed IS the datapackage.
    public JsonElement? GetLocationDataPackage()
    {
        try
        {
            var labels = StandaloneSeed.ListLabels(GameDirectory);
            if (labels.Count == 0) return null;
            var seed = StandaloneSeed.Load(GameDirectory, CurrentStandaloneLabel(labels));
            using var doc = JsonDocument.Parse(JsonSerializer.Serialize(new
            {
                location_name_to_id = seed.NameToId,
            }));
            return doc.RootElement.Clone();
        }
        catch (IOException) { return null; }
        catch (JsonException) { return null; }
    }

    public long[] GetStandaloneLocationUniverse()
    {
        try
        {
            var labels = StandaloneSeed.ListLabels(GameDirectory);
            if (labels.Count == 0) return Array.Empty<long>();
            return StandaloneSeed.Load(GameDirectory, CurrentStandaloneLabel(labels))
                                 .Placements.Keys.ToArray();
        }
        catch (IOException) { return Array.Empty<long>(); }
        catch (JsonException) { return Array.Empty<long>(); }
    }

    public Task<NewsItem[]> GetNewsAsync(CancellationToken ct = default)
        => Task.FromResult(Array.Empty<NewsItem>());

    // --- DeathLink: vehicle crashes travel both ways over the pipe ---

    public bool SendsDeathLink => true;

    public Task OnDeathLinkReceivedAsync(string source, string cause)
        => _pipe?.SendDeathLinkAsync(string.IsNullOrWhiteSpace(cause)
               ? $"{source} died" : cause)
           ?? Task.CompletedTask;

    /// Put the seed's NewGRF sets into OpenTTD's own config before it starts.
    ///
    /// ⚠⚠ THIS IS WHY, and it took four wrong turns to find.
    ///
    /// The mod can build _grfconfig_newgame from slot_data, and it does -- but
    /// something between the world start and the first tick re-reads [newgrf]
    /// from openttd.cfg, which is empty, and the list is gone. Measured:
    ///
    ///     [AP_ConsumeWorldStart] DONE: 3 GRFs in _grfconfig_newgame
    ///     [SessionStart] grf: active=0 newgame=0 scanned=8
    ///
    /// Worse, the lookup ran before OpenTTD's own file scan had finished --
    /// three of eight sets at that moment -- and forcing a rescan there
    /// crashed the game, because the title screen holds pointers into the very
    /// list a rescan rebuilds.
    ///
    /// Writing the config instead sidesteps all of it: the game loads the sets
    /// itself, at startup, through its own machinery, exactly as it does when
    /// a player ticks them by hand. Nothing is mutated mid-flight.
    ///
    /// <param name="slotData">The requirements of the seed being launched.
    /// Passed in rather than read off the field: standalone has its own
    /// slot_data, and the field belongs to whatever join came last.</param>
    private void PrepareNewGrfConfig(JsonElement? slotData)
    {
        try
        {
            // The list is built FIRST, before anything that can bail out.
            // Composing it needs nothing from disk beyond the game folder, and
            // the one time an early return sat above these lines the mod's own
            // GRFs were skipped for every seed that required none of its own.
            var sets = new List<(string GrfId, string Name)>();

            // ⚠⚠ THE MOD'S OWN GRFs COME FIRST, ALWAYS.
            //
            // These two define the map objects a ruin and a star are drawn as.
            // The mod adds them to _grfconfig_newgame itself — and then world
            // generation calls ResetGRFConfig, which rebuilds the list from
            // openttd.cfg and throws them away again. Measured in a real
            // session:
            //
            //   [AP_ConsumeWorldStart] DONE: 4 GRFs in _grfconfig_newgame
            //     GRF: archipelago_ruins.grf ...
            //   [SessionStart] grf: active=3        <- ruins and stars gone
            //   [AP] WARNING: No ruin ObjectTypes found! GRFID=0x55525041
            //   [AP] WARNING: Cannot spawn ruin — no ruin ObjectTypes resolved
            //
            // With a 400-ruin pool that is 400 checks that can never appear.
            // Writing them into the config is what makes them survive the
            // reset, exactly as the seed's own sets do.
            //
            // ⚠ Composed BEFORE anything that can return early, and taken from
            // NewGrfEnabler.OwnSets so the rule has one home and a gate can
            // check it. Two literal lines here are what got skipped last time.
            sets.AddRange(NewGrfEnabler.OwnSets(GameDirectory));
            foreach (var (id, file) in NewGrfEnabler.OwnGrfs)
                if (!sets.Any(s => s.GrfId == id))
                    Log($"{file} is not in the game's newgrf folder.");

            // ⚠ EnsureConfig, not FindConfig: a game that has never run has no
            // openttd.cfg at all, and returning here left the mod's own ruin
            // and star sets switched off for the player's very first seed.
            string? cfg = NewGrfEnabler.EnsureConfig(GameDirectory);
            if (cfg == null) { Log("no openttd.cfg could be written; cannot pre-select NewGRFs"); return; }

            var required = slotData.HasValue
                ? NewGrfRequirements.FromSlotData(slotData.Value)
                : new List<NewGrfRequirement>();

            var onDisk = ScanInstalledSets();
            foreach (var req in required)
            {
                var hit = NewGrfScanner.Find(onDisk, req.GrfId);
                if (hit == null)
                {
                    Log($"{req.DisplayName} is not on disk; the seed asks for it.");
                    continue;
                }
                sets.Add((req.GrfId, NewGrfEnabler.GameRelativeName(hit.Path)));
            }
            if (sets.Count == 0) return;

            string? err = NewGrfEnabler.Enable(cfg, sets, Log);
            if (err != null) Log("could not pre-select NewGRFs: " + err);
        }
        catch (Exception e) { Log("NewGRF pre-select skipped: " + e.Message); }
    }

    // --- What the game page draws ---

    // A few sets seeds commonly ask for. ⚠⚠ THIS IS NOT THE SEED'S LIST — the
    // launcher cannot know that until it connects and reads slot_data.
    //
    // Three green ticks here once sat directly above a seed being refused for
    // three DIFFERENT sets, and read as "everything is fine". Hence the wording
    // below: each badge says only that a FILE is on disk, and says out loud
    // that a file on disk is not a set the game has loaded.
    private static readonly (string GrfId, string Name, string Url)[] KnownSets =
    {
        ("43411223", "Iron Horse (trains)",      "https://grf.farm/iron-horse"),
        ("f1250009", "FIRS (industries)",        "https://grf.farm/firs"),
        ("4c480101", "Aircraft Pack 2025",       "https://www.tt-forums.net"),
    };

    ///
    /// Every NewGRF the game can see, from BOTH places it keeps them.
    ///
    /// ⚠ A set the player installs by hand is a .grf in newgrf/. A set the
    /// game downloads is a .tar under data/content_download/newgrf/ -- a
    /// different folder AND a different shape. Scanning only the first meant
    /// a set could be installed, visible to the game, and still reported
    /// missing here: the badge said "!" and the launcher offered to fetch
    /// what was already there.
    ///
    private IReadOnlyList<NewGrfInfo> ScanInstalledSets()
    {
        var all = new List<NewGrfInfo>();
        all.AddRange(NewGrfScanner.ScanFolder(Path.Combine(GameDirectory, "newgrf")));
        all.AddRange(NewGrfScanner.ScanFolder(
            Path.Combine(GameDirectory, "data", "content_download", "newgrf")));
        return all;
    }

    public IReadOnlyList<GameComponent> DetectComponents()
    {
        var found = ScanInstalledSets();
        return KnownSets.Select(s =>
        {
            var hit = NewGrfScanner.Find(found, s.GrfId);
            return new GameComponent(
                s.Name,
                Present: hit != null,
                ComponentNeed.Optional,
                hit != null
                    ? $"On disk (build {hit.Version}) — not necessarily enabled"
                    : "Not on disk",
                Advice: hit != null
                    ? "A file on disk is not a set the game loads. Tick it in "
                    + "OpenTTD's NewGRF Settings, or a seed built with it will "
                    + "still be refused."
                    : "Only needed when a seed was generated with this set.",
                Url: s.Url);
        }).ToArray();
    }

    public IReadOnlyList<GameCommand> GetCommands() => new[]
    {
        new GameCommand("✎  Create YAML",
            "Build an Archipelago settings file from the apworld's own options.",
            owner => OpenTTDYamlDialog.ShowFor(owner),
            NeedsInstall: false),

        new GameCommand("⬇  Get missing content",
            "Fetch the optional vehicle and industry sets a seed can need.",
            owner => ShowComponentSetup(owner)),
    };

    // --- IGamePlugin: the component walkthrough ---
    //
    // The badges have always said which sets are missing; this is the button
    // that does something about it. Same hook Diablo II uses, so the launcher
    // offers it in the same place.

    public bool HasComponentSetup => true;

    public void ShowComponentSetup(Window? owner)
        => NewGrfFetchDialog.ShowFor(
               owner,
               Path.Combine(GameDirectory, "openttd.exe"),
               KnownSets.Select(s => (s.GrfId, s.Name, s.Url)),
               IsSetInstalled,
               Log);

    /// A set counts as present when a file carrying its GRF id is in newgrf/.
    /// Same scan DetectComponents uses, so the badges and this dialog can
    /// never disagree about what is there.
    private bool IsSetInstalled(string grfId)
    {
        try
        {
            var found = ScanInstalledSets();
            return NewGrfScanner.Find(found, grfId) != null;
        }
        catch { return false; }
    }

    public IReadOnlyList<KnownIssue> KnownIssues => new[]
    {
        new KnownIssue(
            "The seed is refused at connect with a list of NewGRF sets.",
            "The multiworld was generated with vehicle sets this installation " +
            "does not have enabled.",
            "Install the listed sets and enable them in the game's NewGRF " +
            "settings, then reconnect."),
    };

    public IReadOnlyList<GameCredit> Credits => new[]
    {
        new GameCredit("Original game", "OpenTTD Team (GPL-2.0)", Highlight: true),
        new GameCredit("Archipelago integration", "Solida Games"),
        new GameCredit("Multiworld framework", "Archipelago (archipelago.gg)"),
    };
}
