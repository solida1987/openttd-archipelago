using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace LauncherV2.Plugins.OpenTTD;

///
/// Offer to fetch the NewGRF sets a seed was generated from.
///
/// Until now a missing set ended the join: the launcher said "open OpenTTD and
/// use Check Online Content", which is a chore to do in the middle of joining a
/// multiworld, and easy to get wrong (there are several sets with similar
/// names, and the seed needs a specific one at a specific version).
///
/// ⛔ WE SHIP NOTHING. Pressing Download asks the running game to fetch from
/// BaNaNaS — OpenTTD's own content service, where every package carries one of
/// their declared licences (GPL-2.0/3.0, CC-0, CC BY, CC BY-SA, CC BY-NC-SA or
/// CC BY-NC-ND). The file comes from them, to the player, after the player
/// pressed the button. Same shape as the emulator offer: we name whose work it
/// is and let them decide.
///
/// ⚠ Only what the seed names is fetched. OpenTTD removed "download
/// everything" in 1.11 because a handful of people doing it accounted for 70%
/// of the service's bandwidth; a launcher that grabbed sets speculatively would
/// be exactly that problem with a nicer button.
///
internal sealed class NewGrfOfferDialog : Window
{
    private static readonly Brush Ink   = new SolidColorBrush(Color.FromRgb(0xE8, 0xE8, 0xE0));
    private static readonly Brush Dim   = new SolidColorBrush(Color.FromRgb(0x96, 0x9A, 0x92));
    private static readonly Brush Green = new SolidColorBrush(Color.FromRgb(0x6C, 0xC0, 0x5E));
    private static readonly Brush Panel = new SolidColorBrush(Color.FromRgb(0x16, 0x1A, 0x16));
    private static readonly Brush Sunk  = new SolidColorBrush(Color.FromRgb(0x0F, 0x12, 0x0F));

    private bool _accepted;

    /// True when every set listed is already on the player's disk and only
    /// needs ticking. The wording changes completely: offering to DOWNLOAD a
    /// file somebody already has is what sent one player round the same loop
    /// three times.
    private readonly bool _enableOnly;

    private NewGrfOfferDialog(IReadOnlyList<NewGrfCheckResult> problems)
    {
        _enableOnly = problems.All(p => p.State == NewGrfState.NotEnabled);
        Title = _enableOnly
            ? "This seed needs sets you already have"
            : "This seed needs NewGRF sets you do not have";
        Width = 620;
        SizeToContent = SizeToContent.Height;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Panel;
        ResizeMode = ResizeMode.NoResize;

        var root = new StackPanel { Margin = new Thickness(22) };

        root.Children.Add(new TextBlock
        {
            Text = "Missing content",
            Foreground = Green,
            FontSize = 19,
            FontWeight = FontWeights.Bold,
            Margin = new Thickness(0, 0, 0, 6),
        });

        root.Children.Add(new TextBlock
        {
            Text = "This multiworld was generated with add-on content. Without it "
                 + "the seed holds items for vehicles your game does not have, so "
                 + "it cannot be played as generated.",
            Foreground = Dim,
            TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 0, 0, 14),
        });

        // One line per set, so the player can see which specific ones and why.
        var list = new StackPanel
        {
            Background = Sunk,
            Margin = new Thickness(0, 0, 0, 14),
        };
        foreach (var p in problems)
        {
            var row = new StackPanel { Margin = new Thickness(14, 10, 14, 10) };
            row.Children.Add(new TextBlock
            {
                Text = p.Required.DisplayName,
                Foreground = Ink,
                FontWeight = FontWeights.SemiBold,
            });
            row.Children.Add(new TextBlock
            {
                Text = p.Status,
                Foreground = Dim,
                FontSize = 12,
                TextWrapping = TextWrapping.Wrap,
            });
            list.Children.Add(row);
        }
        root.Children.Add(list);

        root.Children.Add(new TextBlock
        {
            Text = "The launcher can ask OpenTTD to download these from its own "
                 + "content service (BaNaNaS), where they are published by their "
                 + "authors under open licences. Only the sets listed above are "
                 + "fetched — nothing else.",
            Foreground = Dim,
            TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 0, 0, 8),
        });

        root.Children.Add(new TextBlock
        {
            Text = "OpenTTD loads NewGRFs at startup, so you will need to restart "
                 + "the game and enable the sets afterwards.",
            Foreground = Dim,
            FontStyle = FontStyles.Italic,
            TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 0, 0, 18),
        });

        var buttons = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
        };

        var no = new Button
        {
            Content = "Not now",
            Padding = new Thickness(18, 7, 18, 7),
            Margin = new Thickness(0, 0, 10, 0),
            MinWidth = 110,
        };
        no.Click += (_, _) => { _accepted = false; Close(); };

        var yes = new Button
        {
            Content = _enableOnly ? "Enable them" : "Download them",
            Padding = new Thickness(18, 7, 18, 7),
            MinWidth = 150,
            IsDefault = true,
        };
        yes.Click += (_, _) => { _accepted = true; Close(); };

        buttons.Children.Add(no);
        buttons.Children.Add(yes);
        root.Children.Add(buttons);

        Content = root;
    }

    ///
    /// Show the offer. True when the player asked for the download.
    ///
    /// Marshalled onto the UI thread because the caller is a pipe callback on a
    /// background thread — a Window constructed there throws, and the throw
    /// would land inside the join and read as a broken bridge.
    ///
    public static bool Ask(IReadOnlyList<NewGrfCheckResult> problems)
    {
        if (problems.Count == 0) return false;

        Application? app = Application.Current;
        if (app?.Dispatcher == null) return false;   // headless: no one to ask

        return app.Dispatcher.Invoke(() =>
        {
            var dlg = new NewGrfOfferDialog(problems)
            {
                Owner = app.Windows.OfType<Window>().FirstOrDefault(w => w.IsActive)
                        ?? app.MainWindow,
            };
            dlg.ShowDialog();
            return dlg._accepted;
        });
    }
}
