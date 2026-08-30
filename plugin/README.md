# OpenTTD plugin for the Multiworld Launcher

This is the source of `openttd_archipelago.londonplugin`, the plugin the
launcher installs the game through.

The project references the launcher's own solution (`OpenTTD.csproj` points at
the Multiworld Launcher source tree), so it does not build standalone from this
repository alone — check out the launcher next to it, or read the code as is.
`Tools/` holds the plugin's proof programs and lints; they are excluded from
the plugin build.
