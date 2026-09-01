using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace LauncherV2.Plugins.OpenTTD;

///
/// Build an Archipelago settings file without opening a text editor.
///
/// Joining a multiworld means handing the host a YAML, and writing one by hand
/// means copying a template and hoping you spelled 108 keys the way the apworld
/// spells them. A wrong key does not warn you — it fails at generation time, in
/// front of everyone waiting to start.
///
/// So the form is not written by hand either. It is built from
/// OpenTTDYamlOptions.g.cs, which tools/gen_yaml_options.py exports FROM the
/// apworld's own options.py. Add an option to the apworld, re-run the tool, and
/// it appears here with its real key, its real default and its real help text.
///
internal sealed class OpenTTDYamlDialog : Window
{
    private readonly TextBox _name = new() { Text = Environment.UserName, MinWidth = 220 };
    private readonly Dictionary<string, Func<object>> _read = new();

    // OpenTTD's own green rather than Diablo's gold: the two dialogs sit in the
    // same launcher and should not look like the same game.
    private static readonly Brush Ink   = new SolidColorBrush(Color.FromRgb(0xE8, 0xE8, 0xE0));
    private static readonly Brush Dim   = new SolidColorBrush(Color.FromRgb(0x96, 0x9A, 0x92));
    private static readonly Brush Green = new SolidColorBrush(Color.FromRgb(0x6C, 0xC0, 0x5E));
    private static readonly Brush Panel = new SolidColorBrush(Color.FromRgb(0x16, 0x1A, 0x16));

    private OpenTTDYamlDialog()
    {
        // "YAML" by name: it is the word every Archipelago player and host
        // actually uses ("send me your YAML").
        Title = "Create an Archipelago YAML";
        Width = 800; Height = 720;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Panel;

        var root = new DockPanel { Margin = new Thickness(14) };

        var head = new StackPanel();
        head.Children.Add(new TextBlock
        {
            Text = "Your YAML for OpenTTD",
            Foreground = Green, FontSize = 17, FontWeight = FontWeights.Bold,
        });
        head.Children.Add(new TextBlock
        {
            Text = "Save the YAML and send it to whoever is generating the multiworld. "
                 + "Everything below is optional — the defaults are a complete game.",
            Foreground = Dim, TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 2, 0, 6),
        });
        head.Children.Add(new TextBlock
        {
            Text = "The NewGRFs group turns extra vehicle sets on. Anything you enable "
                 + "here, everyone in the seed must install through OpenTTD's Check "
                 + "Online Content — the launcher will refuse to start a seed you are "
                 + "missing a set for, rather than let you find out hours in.",
            Foreground = Dim, TextWrapping = TextWrapping.Wrap, FontSize = 11,
            Margin = new Thickness(0, 0, 0, 10),
        });

        var nameRow = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(0, 0, 0, 10) };
        nameRow.Children.Add(new TextBlock
        {
            Text = "Player name", Foreground = Ink, VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(0, 0, 8, 0), FontWeight = FontWeights.Bold,
        });
        nameRow.Children.Add(_name);
        nameRow.Children.Add(new TextBlock
        {
            Text = "  this is the slot you connect as", Foreground = Dim,
            VerticalAlignment = VerticalAlignment.Center,
        });
        head.Children.Add(nameRow);
        DockPanel.SetDock(head, Dock.Top);
        root.Children.Add(head);

        var buttons = new StackPanel
        {
            Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right,
            Margin = new Thickness(0, 10, 0, 0),
        };
        var save = new Button { Content = "Save YAML…", Padding = new Thickness(16, 6, 16, 6), IsDefault = true };
        var cancel = new Button { Content = "Cancel", Padding = new Thickness(16, 6, 16, 6), Margin = new Thickness(8, 0, 0, 0), IsCancel = true };
        save.Click += (_, _) => Save();
        buttons.Children.Add(save);
        buttons.Children.Add(cancel);
        DockPanel.SetDock(buttons, Dock.Bottom);
        root.Children.Add(buttons);

        var stack = new StackPanel();
        foreach (var group in OpenTTDYamlOptions.All.GroupBy(o => o.Group))
        {
            // Long tails start closed: they are all at sensible defaults and
            // most players never look. NewGRFs stays open — it is the one group
            // with a consequence for everyone else in the seed.
            bool longTail = group.Count() > 12 && group.Key != "NewGRFs";
            var inner = new StackPanel { Margin = new Thickness(10, 4, 0, 8) };
            foreach (var opt in group) inner.Children.Add(Row(opt));

            stack.Children.Add(new Expander
            {
                Header = $"{group.Key}  ({group.Count()})",
                Foreground = Green, FontWeight = FontWeights.Bold,
                IsExpanded = !longTail,
                Margin = new Thickness(0, 0, 0, 4),
                Content = inner,
            });
        }
        root.Children.Add(new ScrollViewer
        {
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
            Content = stack,
        });

        Content = root;
    }

    /// One control per option, chosen by the kind the apworld declared.
    private UIElement Row(OpenTTDYamlOption o)
    {
        var box = new StackPanel { Margin = new Thickness(0, 3, 0, 3) };
        var line = new StackPanel { Orientation = Orientation.Horizontal };

        switch (o.Kind)
        {
            case "choice":
            {
                var cb = new ComboBox { MinWidth = 190, Margin = new Thickness(0, 0, 8, 0) };
                foreach (var c in o.Choices) cb.Items.Add(c.Label);
                cb.SelectedIndex = Math.Max(0, Array.FindIndex(o.Choices, c => c.Value == o.Default));
                _read[o.Key] = () => o.Choices.Length == 0
                    ? o.Default
                    : o.Choices[Math.Max(0, cb.SelectedIndex)].Value;
                line.Children.Add(cb);
                line.Children.Add(Label(o.Display));
                break;
            }
            case "range":
            {
                var tb = new TextBox
                {
                    Text = o.Default.ToString(CultureInfo.InvariantCulture),
                    Width = 90, Margin = new Thickness(0, 0, 8, 0),
                };
                _read[o.Key] = () =>
                {
                    // Out of range is rejected by the apworld with an error the
                    // player cannot act on, so clamp here instead.
                    if (!long.TryParse(tb.Text.Trim(), NumberStyles.Integer,
                                       CultureInfo.InvariantCulture, out long v)) v = o.Default;
                    return Math.Clamp(v, o.Min, o.Max);
                };
                line.Children.Add(tb);
                line.Children.Add(Label($"{o.Display}   ({o.Min}–{o.Max})"));
                break;
            }
            default:
            {
                var chk = new CheckBox
                {
                    IsChecked = o.Default != 0, Foreground = Ink,
                    Content = o.Display, VerticalAlignment = VerticalAlignment.Center,
                };
                _read[o.Key] = () => chk.IsChecked == true;
                line.Children.Add(chk);
                break;
            }
        }

        box.Children.Add(line);
        if (!string.IsNullOrWhiteSpace(o.Help))
        {
            box.Children.Add(new TextBlock
            {
                Text = Shorten(o.Help), Foreground = Dim, FontSize = 11,
                TextWrapping = TextWrapping.Wrap, Margin = new Thickness(2, 0, 0, 0),
                MaxWidth = 700,
            });
        }
        return box;
    }

    private static TextBlock Label(string t) => new()
    {
        Text = t, Foreground = Ink, VerticalAlignment = VerticalAlignment.Center,
    };

    // Full text always — see D2YamlDialog for the reasoning.
    private static string Shorten(string s) => s;

    ///
    /// Emit the file. Written by hand rather than through a YAML library: the
    /// shape is flat and fixed, and Archipelago is strict about it — two spaces
    /// of indent, the game name as the section key, booleans as true/false.
    ///
    private void Save()
    {
        string player = _name.Text.Trim();
        if (player.Length == 0)
        {
            MessageBox.Show(this, "Give yourself a player name first — it is the slot you connect as.",
                            "Name required", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }

        // One implementation of the format -- see BuildYaml.
        string yaml = BuildYaml(player, _read);

        var dlg = new Microsoft.Win32.SaveFileDialog
        {
            Title = "Save your YAML",
            FileName = SafeFileName(player) + ".yaml",
            Filter = "Archipelago YAML (*.yaml)|*.yaml|All files (*.*)|*.*",
            DefaultExt = ".yaml",
        };
        if (dlg.ShowDialog(this) != true) return;

        try
        {
            // LF, no BOM — what every other AP YAML in the wild uses.
            File.WriteAllText(dlg.FileName, yaml, new UTF8Encoding(false));
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, "Could not write the file:\n\n" + ex.Message,
                            "Save failed", MessageBoxButton.OK, MessageBoxImage.Error);
            return;
        }

        try { System.Diagnostics.Process.Start("explorer.exe", $"/select,\"{dlg.FileName}\""); }
        catch (Exception) { /* the file is written either way */ }

        MessageBox.Show(this,
            $"Saved as {Path.GetFileName(dlg.FileName)}.\n\n" +
            "Send it to whoever is generating the multiworld.",
            "YAML saved", MessageBoxButton.OK, MessageBoxImage.Information);
        DialogResult = true;
    }

    private static string SafeFileName(string s)
    {
        foreach (char c in Path.GetInvalidFileNameChars()) s = s.Replace(c, '_');
        return s;
    }

    public static void ShowFor(Window? owner)
    {
        var d = new OpenTTDYamlDialog();
        if (owner != null) d.Owner = owner;
        d.ShowDialog();
    }

    ///
    /// The YAML itself. One implementation, used by the Save button above and
    /// available to anything else that needs the same file.
    ///
    /// ⚠ There used to be two: this one and a copy inside Save(). This one had
    /// a comment claiming the gate called it — the gate is Python and parses
    /// the generated options file itself, so nothing called this at all while
    /// the copy in Save() was what players actually got. Two implementations
    /// of one format drift, and only the unused one was documented.
    ///
    /// <param name="values">What the player chose, by option key. Null uses
    /// each option's default — which is what a gate or a smoke test wants.</param>
    ///
    internal static string BuildYaml(string player,
                                     IReadOnlyDictionary<string, Func<object>>? values = null)
    {
        var sb = new StringBuilder();
        sb.Append("name: ").Append(Quoted(player)).Append('\n');
        sb.Append("description: Created with the Multiworld Launcher\n");
        sb.Append("game: ").Append(OpenTTDYamlOptions.Game).Append('\n');
        sb.Append(OpenTTDYamlOptions.Game).Append(":\n");
        // Two settings every AP file carries; the apworld does not declare them.
        sb.Append("  progression_balancing: normal\n");
        sb.Append("  accessibility: full\n");
        foreach (var o in OpenTTDYamlOptions.All)
        {
            string text;
            if (values != null && values.TryGetValue(o.Key, out var read))
            {
                text = read() switch
                {
                    bool b => b ? "true" : "false",
                    long l => l.ToString(CultureInfo.InvariantCulture),
                    int i  => i.ToString(CultureInfo.InvariantCulture),
                    var v  => v?.ToString() ?? "",
                };
            }
            else if (values != null)
            {
                continue;   // the form did not offer this one
            }
            else
            {
                text = o.Kind == "toggle" ? (o.Default != 0 ? "true" : "false")
                                          : o.Default.ToString(CultureInfo.InvariantCulture);
            }
            sb.Append("  ").Append(o.Key).Append(": ").Append(text).Append('\n');
        }
        return sb.ToString();
    }

    /// A YAML double-quoted scalar. Windows account names carry colons,
    /// hashes and leading braces often enough, and any of them turns a bare
    /// name into a parse error the player is blamed for at generation time.
    private static string Quoted(string s)
        => "\"" + s.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
}
