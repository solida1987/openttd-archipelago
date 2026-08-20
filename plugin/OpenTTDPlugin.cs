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
    public string Subtitle    => "Randomiser Mod";
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
    public event Action<string>? StandaloneItemReceived;
    public event Action<string>? LogLine;

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
    private bool _slotDataSent;
    private readonly HashSet<long> _checkedIds = new();

    // --- Update / install ---

    private const string GitHubOwner = "solida1987";
    private const string GitHubRepo  = "openttd-archipelago";

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
        catch (System.Net.Http.HttpRequestException) { /* offline is not an error state */ }
        catch (JsonException) { }
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
                rel.Equals(Path.Combine("data", "openttd.cfg"), StringComparison.OrdinalIgnoreCase);

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

        // Session state must not leak between launches: a standalone run
        // leaves ITS seed's slot_data here, and sending that to a joined
        // multiworld hands the game the wrong world. The location table
        // stays -- it belongs to the WORLD, not the seed.
        _slotData = null;
        _idToLabel = new Dictionary<long, string>();
        _accepted = false;
        _slotDataSent = false;
        lock (_checkedIds) _checkedIds.Clear();

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

        _game = Process.Start(new ProcessStartInfo
        {
            FileName         = ExePath,
            Arguments        = $"-ap-pipe {pipeName}",
            WorkingDirectory = GameDirectory,
            UseShellExecute  = false,
        }) ?? throw new InvalidOperationException("Could not start openttd.exe.");

        IsRunning = true;
        _game.EnableRaisingEvents = true;
        _game.Exited += (_, _) =>
        {
            IsRunning = false;
            int code = 0;
            try { code = _game?.ExitCode ?? 0; } catch (InvalidOperationException) { }
            GameExited?.Invoke(code);
        };

        await pipe.WaitForGameAsync(linked.Token);
        _ = pipe.RunAsync(_sessionCts.Token);
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

        _game = Process.Start(new ProcessStartInfo
        {
            FileName         = ExePath,
            Arguments        = $"-ap-pipe {pipeName}",
            WorkingDirectory = GameDirectory,
            UseShellExecute  = false,
        }) ?? throw new InvalidOperationException("Could not start openttd.exe.");

        IsRunning = true;
        _game.EnableRaisingEvents = true;
        _game.Exited += (_, _) =>
        {
            IsRunning = false;
            int code = 0;
            try { code = _game?.ExitCode ?? 0; } catch (InvalidOperationException) { }
            GameExited?.Invoke(code);
        };

        await pipe.WaitForGameAsync(linked.Token);
        _ = pipe.RunAsync(_sessionCts.Token);
    }

    private void WireStandaloneEvents(OpenTTDPipeServer pipe, StandaloneSeed seed, StandaloneState state)
    {
        var done = new HashSet<long>(state.CheckedInOrder);

        pipe.LogReported += text => Log("game: " + text);

        pipe.GrfListReceived += async loaded =>
        {
            Log($"game reports {loaded.Count} NewGRF(s): " +
                (loaded.Count == 0 ? "none"
                                   : string.Join(", ", loaded.Select(g => $"{g.GrfId} v{g.Version}"))));
            string? refusal = EvaluateGrfs(seed.SlotData, loaded);
            if (refusal != null)
            {
                Log("standalone refused: " + refusal);
                await pipe.SendRejectAsync(refusal);
                return;
            }

            await pipe.SendSlotDataAsync(seed.SlotData);
            await pipe.SendLocationCountAsync(seed.Placements.Count);
            await pipe.SendCheckedAsync(
                done.Where(seed.IdToName.ContainsKey).Select(id => seed.IdToName[id]));

            // Replay what this run already earned; the index dedups.
            int index = 0;
            foreach (long loc in state.CheckedInOrder)
                if (seed.Placements.TryGetValue(loc, out long item))
                    await pipe.SendItemAsync(item, index++);

            await pipe.SendStateAsync(3);
            Log($"standalone accepted; {done.Count} checks replayed");
        };

        pipe.CheckReported += async id =>
        {
            if (!seed.Placements.TryGetValue(id, out long item) || !done.Add(id)) return;

            state.CheckedInOrder.Add(id);
            state.Save();
            await pipe.SendItemAsync(item, state.CheckedInOrder.Count - 1);

            LocationsChecked?.Invoke(new[] { id });
            string locName  = seed.IdToName.TryGetValue(id, out var ln) ? ln : id.ToString();
            string itemName = seed.ItemNames.TryGetValue(item, out var inm) ? inm : item.ToString();
            StandaloneItemReceived?.Invoke($"{locName}: {itemName}");
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
    public void OnApServicesAttached(IApServices? services) => _ap = services;

    private void WireEvents(OpenTTDPipeServer pipe)
    {
        pipe.CheckReported += id => LocationsChecked?.Invoke(new[] { id });
        pipe.DeathReported += cause => _ap?.ReportDeath(cause);
        pipe.GoalReported  += () => { Log("goal reached"); GoalCompleted?.Invoke(); };
        pipe.LogReported   += text => Log("game: " + text);

        pipe.GrfListReceived += async loaded =>
        {
            Log($"game reports {loaded.Count} NewGRF(s): " +
                (loaded.Count == 0 ? "none"
                                   : string.Join(", ", loaded.Select(g => $"{g.GrfId} v{g.Version}"))));

            string? refusal = _slotData.HasValue
                ? EvaluateGrfs(_slotData.Value, loaded) : null;
            if (refusal != null)
            {
                Log("refused: " + refusal);
                await pipe.SendRejectAsync(refusal);
                return;
            }

            Log(_slotData.HasValue
                ? $"accepted; sending slot_data and {_idToName.Count} locations"
                : "accepted, but no slot_data has arrived yet");

            _accepted = true;
            if (_slotData.HasValue)
            {
                await pipe.SendSlotDataAsync(_slotData.Value);
                _slotDataSent = true;
            }
            if (_idToName.Count > 0) await pipe.SendLocationCountAsync(_idToName.Count);
            await PushCheckedAsync(pipe);
            await pipe.SendStateAsync(StateNumber(_apState));

            // Replay the item stream from index 0. Items delivered while the
            // game was still starting had no pipe to land in; the index lets
            // the game recognise what it already handled.
            if (_ap != null) await _ap.ResyncAsync();
        };

        pipe.ScoutRequested += async () =>
        {
            foreach (var (id, label) in _idToLabel)
                if (_idToName.TryGetValue(id, out string? name))
                    await pipe.SendHintAsync(name, label);
        };
    }

    ///
    /// Does the player have what this seed was built from?
    ///
    /// The list comes from the game rather than from a folder scan on purpose:
    /// a file on disk that the player has not enabled is not loaded, and the
    /// game is the only side that knows the difference.
    ///
    /// null when everything needed is loaded, otherwise what to tell them.
    private static string? EvaluateGrfs(JsonElement slotData,
                                        IReadOnlyList<OpenTTDPipeServer.LoadedGrf> loaded)
    {
        var required = NewGrfRequirements.FromSlotData(slotData);
        if (required.Count == 0) return null;

        // The scanner's shape, filled from what the game said. Path and name
        // stay empty: the launcher never saw these as files.
        var installed = loaded.Select(g => new NewGrfInfo(
            Path: "", GrfId: g.GrfId, Name: null, Description: null,
            Version: g.Version, MinVersion: null, Url: null, Error: null));

        return NewGrfRequirements.Explain(NewGrfRequirements.Evaluate(installed, required));
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
            _ = pipe.SendLocationCountAsync(_idToName.Count);
            _ = PushCheckedAsync(pipe);
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
        // mid-game would reset the game's mission completion flags.
        var pipe = _pipe;
        if (pipe != null && _accepted && !_slotDataSent)
        {
            _slotDataSent = true;
            _ = pipe.SendSlotDataAsync(_slotData.Value);
        }
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
            try { if (!game.HasExited) game.CloseMainWindow(); }
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

    // The tracker's world: in standalone the chosen seed IS the datapackage.
    public JsonElement? GetLocationDataPackage()
    {
        try
        {
            var labels = StandaloneSeed.ListLabels(GameDirectory);
            if (labels.Count == 0) return null;
            string chosen = Core.PluginSettings.Get(GameId, "standalone_seed") ?? labels[0];
            if (!labels.Contains(chosen)) chosen = labels[0];
            var seed = StandaloneSeed.Load(GameDirectory, chosen);
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
            string chosen = Core.PluginSettings.Get(GameId, "standalone_seed") ?? labels[0];
            if (!labels.Contains(chosen)) chosen = labels[0];
            return StandaloneSeed.Load(GameDirectory, chosen).Placements.Keys.ToArray();
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

    // --- What the game page draws ---

    // The vehicle sets a seed may ask for. The player installs them himself in
    // the game's own NewGRF window; these badges only say what a scan of
    // newgrf/ can see. The session-time guard against slot_data stays the
    // real gate -- a file on disk is not necessarily enabled.
    private static readonly (string GrfId, string Name, string Url)[] KnownSets =
    {
        ("43411223", "Iron Horse (trains)",      "https://grf.farm/iron-horse"),
        ("f1250009", "FIRS (industries)",        "https://grf.farm/firs"),
        ("4c480101", "Aircraft Pack 2025",       "https://www.tt-forums.net"),
    };

    public IReadOnlyList<GameComponent> DetectComponents()
    {
        var found = NewGrfScanner.ScanFolder(Path.Combine(GameDirectory, "newgrf"));
        return KnownSets.Select(s =>
        {
            var hit = NewGrfScanner.Find(found, s.GrfId);
            return new GameComponent(
                s.Name,
                Present: hit != null,
                ComponentNeed.Optional,
                hit != null ? $"Found (build {hit.Version})"
                            : "Not in the newgrf folder",
                Advice: hit != null
                    ? "Remember to enable it in the game's NewGRF settings."
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
    };

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
