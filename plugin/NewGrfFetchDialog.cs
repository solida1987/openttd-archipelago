using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace LauncherV2.Plugins.OpenTTD;

///
/// "Get missing content" — the vehicle and industry sets a seed can be built
/// from, fetched on request.
///
/// The game page has always shown which sets are missing; it just had no way
/// to do anything about it, so the answer was "open OpenTTD, find Check Online
/// Content, and hunt for the right one". This is the same button Diablo II has
/// for its components, and it works the same way: one screen, whose work it
/// is, what licence, and a button the player presses themselves.
///
/// ⛔ THE LAUNCHER DOWNLOADS NOTHING ITSELF. It starts OpenTTD with
/// -ap-fetch-grf, and the game fetches from BaNaNaS with the content client it
/// already ships. Reimplementing that protocol here would mean a third-party
/// client impersonating theirs against their service, and it would rot the
/// first time they changed it. Names, authors and licences shown below come
/// from their public API — read-only, for credit.
///
/// ⚠ Only the sets listed are fetched. OpenTTD removed "download everything"
/// in 1.11 because a few people doing it accounted for 70% of the service's
/// bandwidth.
///
internal sealed class NewGrfFetchDialog : Window
{
    private static readonly Brush Ink   = new SolidColorBrush(Color.FromRgb(0xE8, 0xE8, 0xE0));
    private static readonly Brush Dim   = new SolidColorBrush(Color.FromRgb(0x96, 0x9A, 0x92));
    private static readonly Brush Green = new SolidColorBrush(Color.FromRgb(0x6C, 0xC0, 0x5E));
    private static readonly Brush Panel = new SolidColorBrush(Color.FromRgb(0x16, 0x1A, 0x16));
    private static readonly Brush Sunk  = new SolidColorBrush(Color.FromRgb(0x0F, 0x12, 0x0F));

    private static readonly HttpClient Http = new() { Timeout = TimeSpan.FromSeconds(15) };

    /// One set as the player sees it: our name for it, plus whatever the
    /// content service says about who made it.
    private sealed record SetInfo(string GrfId, string Name, string Url)
    {
        public string? Author  { get; set; }
        public string? Licence { get; set; }
        public string? Version { get; set; }
        public long    Bytes   { get; set; }
    }

    private readonly List<SetInfo> _sets;
    private readonly string _gameExe;
    private readonly Action<string> _log;
    private readonly StackPanel _list = new();
    private readonly TextBlock _status = new()
    {
        Foreground = Dim, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 10, 0, 0),
    };
    private Button _fetch = null!;

    private NewGrfFetchDialog(List<SetInfo> sets, string gameExe, Action<string> log)
    {
        _sets = sets; _gameExe = gameExe; _log = log;

        Title = "Get missing content";
        Width = 640;
        SizeToContent = SizeToContent.Height;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Panel;
        ResizeMode = ResizeMode.NoResize;

        var root = new StackPanel { Margin = new Thickness(22) };

        root.Children.Add(new TextBlock
        {
            Text = "Optional vehicle and industry sets",
            Foreground = Green, FontSize = 19, FontWeight = FontWeights.Bold,
            Margin = new Thickness(0, 0, 0, 6),
        });
        root.Children.Add(new TextBlock
        {
            Text = "Seeds can be generated with these. You only need a set if a seed "
                 + "asks for it — but a seed that does will not start without it.",
            Foreground = Dim, TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 0, 0, 14),
        });

        _list.Background = Sunk;
        _list.Margin = new Thickness(0, 0, 0, 14);
        root.Children.Add(_list);
        Redraw();

        root.Children.Add(new TextBlock
        {
            Text = "OpenTTD fetches these itself, from its own content service. "
                 + "The launcher opens the game briefly to do it and closes it again — "
                 + "nothing is downloaded by us, and nothing else is fetched.",
            Foreground = Dim, TextWrapping = TextWrapping.Wrap, FontSize = 12,
            Margin = new Thickness(0, 0, 0, 4),
        });
        root.Children.Add(new TextBlock
        {
            Text = "OpenTTD loads sets at startup, so enable them in its NewGRF "
                 + "settings the next time you play.",
            Foreground = Dim, TextWrapping = TextWrapping.Wrap, FontSize = 12,
            FontStyle = FontStyles.Italic,
        });
        root.Children.Add(_status);

        var buttons = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Margin = new Thickness(0, 16, 0, 0),
        };
        var close = new Button
        {
            Content = "Close", Padding = new Thickness(18, 7, 18, 7),
            Margin = new Thickness(0, 0, 10, 0), MinWidth = 110, IsCancel = true,
        };
        _fetch = new Button
        {
            Content = "Download the missing ones",
            Padding = new Thickness(18, 7, 18, 7), MinWidth = 190, IsDefault = true,
        };
        _fetch.Click += async (_, _) => await FetchAsync();
        buttons.Children.Add(close);
        buttons.Children.Add(_fetch);
        root.Children.Add(buttons);

        Content = root;
        Loaded += async (_, _) => await LoadDetailsAsync();
    }

    private void Redraw()
    {
        _list.Children.Clear();
        foreach (var s in _sets)
        {
            var row = new StackPanel { Margin = new Thickness(14, 10, 14, 10) };
            row.Children.Add(new TextBlock
            {
                Text = s.Name, Foreground = Ink, FontWeight = FontWeights.SemiBold,
            });

            var by = s.Author is { Length: > 0 }
                ? $"by {s.Author}" : "by its author";
            var lic = s.Licence is { Length: > 0 } ? $"   ·   {s.Licence}" : "";
            var ver = s.Version is { Length: > 0 } ? $"   ·   v{s.Version}" : "";
            var size = s.Bytes > 0 ? $"   ·   {s.Bytes / 1048576.0:0.#} MB" : "";
            row.Children.Add(new TextBlock
            {
                Text = by + lic + ver + size,
                Foreground = Dim, FontSize = 12, TextWrapping = TextWrapping.Wrap,
            });
            row.Children.Add(new TextBlock
            {
                Text = s.Url, Foreground = Dim, FontSize = 11,
                TextWrapping = TextWrapping.Wrap,
            });
            _list.Children.Add(row);
        }
    }

    /// Ask the content service who made these and under what licence.
    /// Read-only and best effort: if it does not answer, the dialog still
    /// works, it just credits them less precisely.
    private async Task LoadDetailsAsync()
    {
        foreach (var s in _sets)
        {
            try
            {
                string json = await Http.GetStringAsync(
                    $"https://bananas-api.openttd.org/package/newgrf/{s.GrfId}");
                using var doc = JsonDocument.Parse(json);
                var root = doc.RootElement;

                if (root.TryGetProperty("authors", out var authors) &&
                    authors.ValueKind == JsonValueKind.Array)
                {
                    var names = authors.EnumerateArray()
                        .Select(a => a.TryGetProperty("display-name", out var n) ? n.GetString() : null)
                        .Where(n => !string.IsNullOrWhiteSpace(n));
                    s.Author = string.Join(", ", names);
                }
                if (root.TryGetProperty("versions", out var versions) &&
                    versions.ValueKind == JsonValueKind.Array &&
                    versions.GetArrayLength() > 0)
                {
                    var latest = versions[versions.GetArrayLength() - 1];
                    if (latest.TryGetProperty("license", out var l)) s.Licence = l.GetString();
                    if (latest.TryGetProperty("version", out var v)) s.Version = v.GetString();
                    if (latest.TryGetProperty("filesize", out var f) && f.TryGetInt64(out long b))
                        s.Bytes = b;
                }
            }
            catch (Exception ex)
            {
                _log($"newgrf lookup for {s.GrfId} failed: {ex.Message}");
            }
        }
        Redraw();
    }

    private async Task FetchAsync()
    {
        if (!File.Exists(_gameExe))
        {
            _status.Text = "OpenTTD is not installed yet — install the game first.";
            return;
        }

        _fetch.IsEnabled = false;
        _status.Foreground = Ink;
        _status.Text = "OpenTTD is fetching the sets… this can take a minute for a large one.";

        string ids = string.Join(",", _sets.Select(s => s.GrfId));
        _log($"starting OpenTTD to fetch: {ids}");

        try
        {
            var psi = new ProcessStartInfo
            {
                FileName = _gameExe,
                Arguments = $"-ap-fetch-grf {ids}",
                WorkingDirectory = Path.GetDirectoryName(_gameExe)!,
                UseShellExecute = false,
            };
            using var p = Process.Start(psi);
            if (p == null) { _status.Text = "Could not start OpenTTD."; _fetch.IsEnabled = true; return; }
            await p.WaitForExitAsync();
        }
        catch (Exception ex)
        {
            _status.Text = "Could not start OpenTTD: " + ex.Message;
            _fetch.IsEnabled = true;
            return;
        }

        // Say what actually landed rather than "done": the service may not have
        // had one, and a green message over a missing file is the worst answer.
        var still = _sets.Where(s => !IsInstalled(s.GrfId)).ToList();
        if (still.Count == 0)
        {
            _status.Foreground = Green;
            _status.Text = "All set. Enable them in OpenTTD's NewGRF settings the next time you play.";
        }
        else
        {
            _status.Foreground = Ink;
            _status.Text = "Still missing: " + string.Join(", ", still.Select(s => s.Name))
                         + ". They may not be on the content service under that id — "
                         + "the page listed above is where they come from.";
        }
        _fetch.IsEnabled = true;
    }

    /// Both places the game keeps sets: hand-installed .grf files, and the
    /// .tar archives its own downloader writes. See ScanInstalledSets in the
    /// plugin for why looking in only one of them was wrong.
    private bool IsInstalled(string grfId)
    {
        try
        {
            string root = Path.GetDirectoryName(_gameExe) ?? ".";
            var all = new List<NewGrfInfo>();
            all.AddRange(NewGrfScanner.ScanFolder(Path.Combine(root, "newgrf")));
            all.AddRange(NewGrfScanner.ScanFolder(
                Path.Combine(root, "data", "content_download", "newgrf")));
            return NewGrfScanner.Find(all, grfId) != null;
        }
        catch { return false; }
    }

    ///
    /// Show the dialog for whatever is not installed. When everything is
    /// already there, say so instead of opening an empty list.
    ///
    public static void ShowFor(Window? owner, string gameExe,
                               IEnumerable<(string GrfId, string Name, string Url)> knownSets,
                               Func<string, bool> isInstalled,
                               Action<string> log)
    {
        var missing = knownSets.Where(s => !isInstalled(s.GrfId))
                               .Select(s => new SetInfo(s.GrfId, s.Name, s.Url))
                               .ToList();
        if (missing.Count == 0)
        {
            MessageBox.Show(owner,
                "You already have every optional set the launcher knows about.\n\n"
                + "Remember that OpenTTD only loads a set once you enable it in "
                + "its NewGRF settings.",
                "Nothing missing", MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }

        var dlg = new NewGrfFetchDialog(missing, gameExe, log) { Owner = owner };
        dlg.ShowDialog();
    }
}
