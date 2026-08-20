using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Pipes;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace LauncherV2.Plugins.OpenTTD;

///
/// The launcher end of docs/ap_pipe_protocol.md.
///
/// OpenTTD used to hold its own Archipelago connection. It no longer does: the
/// launcher owns the connection and the game speaks one line at a time over a
/// named pipe. Everything here is that translation — game lines in, AP calls
/// out, and AP events back down the pipe.
///
/// The game cannot tell whether a real multiworld or a local seed is on the
/// other side. That is deliberate: it is what makes standalone free.
///
internal sealed class OpenTTDPipeServer : IAsyncDisposable
{
    private readonly NamedPipeServerStream _pipe;
    private readonly CancellationTokenSource _cts = new();

    // PipeStream does not promise atomic interleaving, and lines arrive here
    // from the AP client's threads as well as the read loop.
    private readonly SemaphoreSlim _writeLock = new(1, 1);

    /// Location name to AP id, from the datapackage. Empty until it lands.
    private IReadOnlyDictionary<string, long> _nameToId =
        new Dictionary<string, long>(StringComparer.Ordinal);

    public string PipeName { get; }

    /// A location the game reported, already resolved to an AP id.
    public event Action<long>? CheckReported;

    /// The game says the seed is finished.
    public event Action? GoalReported;

    /// The game wants labels for its shop slots.
    public event Action? ScoutRequested;

    /// Chat out of the game.
    public event Action<string>? SayReported;

    /// The player died, and DeathLink is on.
    public event Action<string>? DeathReported;

    /// Free-text from the game, for the log. Not shown to the player.
    public event Action<string>? LogReported;

    ///
    /// The NewGRFs the game reports as loaded, once GRFEND: has arrived.
    /// The launcher compares these with the seed's requirements before it lets
    /// play start.
    ///
    public event Action<IReadOnlyList<LoadedGrf>>? GrfListReceived;

    /// A NewGRF the game has loaded: id as hex text, plus its Action 14 version.
    public readonly record struct LoadedGrf(string GrfId, uint Version);

    public OpenTTDPipeServer(string pipeName)
    {
        PipeName = pipeName;
        // Byte mode, not Message: the game frames on '\n' itself.
        _pipe = new NamedPipeServerStream(pipeName, PipeDirection.InOut,
            maxNumberOfServerInstances: 1,
            transmissionMode: PipeTransmissionMode.Byte,
            options: PipeOptions.Asynchronous);
    }

    public Task WaitForGameAsync(CancellationToken ct)
    {
        using var linked = CancellationTokenSource.CreateLinkedTokenSource(ct, _cts.Token);
        return _pipe.WaitForConnectionAsync(linked.Token);
    }

    public void SetLocationTable(IReadOnlyDictionary<string, long> nameToId) => _nameToId = nameToId;

    // ── Launcher to game ────────────────────────────────────────────────────

    public Task SendStateAsync(int state)          => SendAsync($"STATE:{state}");
    public Task SendErrorAsync(string text)        => SendAsync("ERROR:" + OneLine(text));
    public Task SendRejectAsync(string text)       => SendAsync("REJECT:" + OneLine(text));
    public Task SendItemAsync(long id, int index)  => SendAsync($"ITEM:{id}:{index}");
    public Task SendDeathLinkAsync(string cause)   => SendAsync("DEATHLINK:" + OneLine(cause));
    public Task SendPrintAsync(string text)        => SendAsync("PRINT:" + OneLine(text));
    public Task SendLocationCountAsync(int n)      => SendAsync($"LOCCOUNT:{n}");
    /// The multiworld's seed name — the game keys its per-seed savegame on it.
    public Task SendSeedAsync(string name)         => SendAsync("SEED:" + OneLine(name));
    public Task SendHintAsync(string location, string label)
        => SendAsync($"HINT:{location}:{OneLine(label)}");

    /// slot_data as one line. The game reads it as raw JSON.
    public Task SendSlotDataAsync(JsonElement slotData)
        => SendAsync("SLOTDATA:" + slotData.GetRawText().Replace("\r", "").Replace("\n", ""));

    /// Locations already checked, by NAME — the game holds no id table.
    /// This is the resume sync: missions, stars and shop slots it names are
    /// marked done in-game instead of being offered again.
    public Task SendCheckedAsync(IEnumerable<string> names)
        => SendAsync("CHECKED:" + string.Join(",", names.Select(OneLine)));

    public Task SendPlayersAsync(IEnumerable<string> names)
        => SendAsync("PLAYERS:" + string.Join(",", names.Select(OneLine)));

    // ── Game to launcher ────────────────────────────────────────────────────

    /// Read lines until the game closes the pipe or we are torn down.
    public async Task RunAsync(CancellationToken ct)
    {
        using var linked = CancellationTokenSource.CreateLinkedTokenSource(ct, _cts.Token);
        var buffer = new byte[64 * 1024];
        var pending = new StringBuilder();
        var grfs = new List<LoadedGrf>();

        while (!linked.IsCancellationRequested)
        {
            int read;
            try
            {
                read = await _pipe.ReadAsync(buffer.AsMemory(), linked.Token);
            }
            catch (OperationCanceledException) { break; }
            catch (IOException) { break; }   // the game closed the pipe
            if (read == 0) break;            // and so is this

            pending.Append(Encoding.UTF8.GetString(buffer, 0, read));
            while (true)
            {
                string all = pending.ToString();
                int nl = all.IndexOf('\n');
                if (nl < 0) break;
                pending.Clear();
                pending.Append(all[(nl + 1)..]);
                HandleLine(all[..nl].TrimEnd('\r'), grfs);
            }
        }
    }

    private void HandleLine(string line, List<LoadedGrf> grfs)
    {
        if (line.Length == 0) return;
        int colon = line.IndexOf(':');
        if (colon < 0) return;

        string tag = line[..colon];
        string body = line[(colon + 1)..];

        switch (tag)
        {
            case "HELLO":
                // Protocol version. Nothing to do while there is only one.
                break;

            case "GRF":
            {
                // GRF:<grfid hex>:<version>
                int sep = body.IndexOf(':');
                if (sep < 0) break;
                _ = uint.TryParse(body[(sep + 1)..], out uint version);
                grfs.Add(new LoadedGrf(body[..sep], version));
                break;
            }

            case "GRFEND":
                GrfListReceived?.Invoke(grfs.ToArray());
                grfs.Clear();
                break;

            case "CHECK":
                if (long.TryParse(body, out long id)) CheckReported?.Invoke(id);
                break;

            case "CHECKNAME":
                // The game holds no id table, so this is the normal path. An
                // unknown name is dropped rather than guessed at: sending the
                // wrong id would check somebody else's location.
                if (_nameToId.TryGetValue(body, out long byName)) CheckReported?.Invoke(byName);
                else LogReported?.Invoke("unknown location from game: " + body);
                break;

            case "GOAL":  GoalReported?.Invoke(); break;
            case "SCOUT": ScoutRequested?.Invoke(); break;
            case "SAY":   SayReported?.Invoke(body); break;
            case "DEATH": DeathReported?.Invoke(body); break;
            case "LOG":   LogReported?.Invoke(body); break;

            // Unknown tags are ignored on purpose: it lets either side gain a
            // message before the other learns about it.
        }
    }

    // ── Plumbing ────────────────────────────────────────────────────────────

    /// Newlines would split one message into two. Nothing we send needs them.
    private static string OneLine(string s) => s.Replace("\r", " ").Replace("\n", " ");

    private async Task SendAsync(string line)
    {
        if (!_pipe.IsConnected) return;
        byte[] bytes = Encoding.UTF8.GetBytes(line + "\n");
        await _writeLock.WaitAsync();
        try
        {
            await _pipe.WriteAsync(bytes.AsMemory(), _cts.Token);
            await _pipe.FlushAsync(_cts.Token);
        }
        catch (OperationCanceledException) { }
        catch (IOException) { }               // game gone; the read loop reports it
        catch (ObjectDisposedException) { }
        finally { _writeLock.Release(); }
    }

    public async ValueTask DisposeAsync()
    {
        try { _cts.Cancel(); } catch (ObjectDisposedException) { }
        try { if (_pipe.IsConnected) _pipe.Disconnect(); } catch (InvalidOperationException) { }
        await _pipe.DisposeAsync();
        _cts.Dispose();
        _writeLock.Dispose();
    }
}
