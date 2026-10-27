# IBus engine

NexKey ships a native IBus engine next to the Fcitx5 addon, so it can be used
with either input method framework.

```
integrations/ibus/nexkey-ibus.cpp   engine implementation (GObject + nexkey-core)
integrations/ibus/data/nexkey.xml   component file, filled in at configure time
```

## How it works

An IBus engine is a separate process that connects to the IBus daemon on the
session bus. The component file below
`/usr/share/ibus/component/nexkey.xml` tells IBus what to start and which
engines the binary provides. IBus starts the engine by absolute path, so the
installed copy of that file is rewritten at install time with the prefix the
files were actually installed under.

| Piece | Purpose |
| --- | --- |
| `main()` | connects to the daemon, registers the engine type on the factory and requests the name `org.freedesktop.IBus.NexKey` |
| `session()` | one `nexkey::Engine` plus the configuration, reloaded when the file changes |
| `process_key_event` | toggle shortcut, candidate selection, backspace, then the ordinary keystroke path |
| preedit | published with `ibus_engine_update_preedit_text_with_mode` and committed on focus loss |
| lookup table | the candidate window, filled from `nexkey::Engine::candidates()` |
| properties | a `NexKey` menu in the IBus panel with a VI/EN radio item |

The engine is a thin wrapper: everything about Vietnamese text lives in
`nexkey-core` and is covered by the unit tests, so the Fcitx5 addon and the
IBus engine produce the same text.

The one deliberate difference is how the text reaches the application. The
Fcitx5 addon types straight into the document and steps down when an
application drops a replacement (see
[docs/ARCHITECTURE.md](ARCHITECTURE.md#direct-typing-and-the-delivery-ladder)),
while the IBus engine always uses the preedit and commits each word once. That
is the most compatible path there is, because nothing is ever deleted, so
`engine.direct_typing` has no effect here. An application that shows the
underline badly in IBus gets the same behaviour in Fcitx5 when its rule is
`preedit`.

## Install

```bash
sudo cmake --install build --prefix /usr    # installs both backends
ibus write-cache --system                   # refresh the component cache
ibus restart                                # or log out and back in
```

Then open the IBus preferences (`ibus-setup`), go to *Input Method* and add
**NexKey**. The Debian package does the cache refresh for you.

Requirements: `libibus-1.0-dev` to build, `ibus` to run. Configure with
`-DNEXKEY_BUILD_IBUS=OFF` to skip the engine.

## Switching modes

| Control | Effect |
| --- | --- |
| the configured `ui.toggle_key`, for example `Control+Shift` | switches between VI and EN |
| the *NexKey* menu in the IBus panel | same, through the radio item |
| `nexkey toggle` in a terminal | same, through the configuration file |

The mode is written to `~/.config/nexkey/nexkey.conf`, so the Fcitx5 addon, the
tray application and the IBus engine always agree.

A modifier only shortcut such as `Control+Shift` reacts to the modifier key
itself, a full combination such as `Ctrl+Shift+space` reacts to that
combination. Keep in mind that IBus has its own trigger key (usually
`Control+space`); pick a different combination for NexKey or change the IBus
trigger, otherwise IBus consumes the key before the engine sees it.

## Candidate window

The list appears while a word has more than one reading:

- `1` to `9` pick an entry in Telex and Simple Telex;
- in VNI and VIQR the digits are tone markers and literal text, so selection
  uses `Ctrl` plus a digit or a mouse click;
- `Up` and `Down` move the cursor inside the list;
- typing a separator, `Enter`, `Escape` or `Tab` commits the default entry.

Disable the window with `nexkey config set engine.candidates false`.

## IBus and Fcitx5 side by side

Both backends read the same configuration file, so a user may install both and
choose at login which framework starts. They do not share runtime state: use one
framework at a time, which is the normal setup on Linux desktops.

The AppImage bundles the Fcitx5 addon only. An IBus component file has to point
at an absolute path of the engine binary, which does not survive inside an
AppImage, so IBus support is provided by the system package.

## Troubleshooting

| Symptom | Cause and fix |
| --- | --- |
| NexKey is not in the IBus list | the component cache is stale: run `ibus write-cache --system` and restart IBus |
| `cannot connect to the IBus daemon` | IBus is not running, or `IBUS_ADDRESS` does not point at a session |
| the toggle shortcut does nothing | the shortcut clashes with the IBus trigger key, or `ui.toggle_key` is empty |
| text is committed but not composed | another engine is active, check the panel indicator |
| the engine starts and exits immediately | another process already owns `org.freedesktop.IBus.NexKey` |

Logs go to the usual IBus log, for example:

```bash
ibus-daemon -drx -v    # verbose
journalctl --user -u ibus -f
```
