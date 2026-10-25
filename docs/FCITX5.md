# Fcitx5 addon

NexKey speaks Fcitx5 natively: the engine is an addon that Fcitx5 loads when the
NexKey input method is selected. There is no helper process and nothing to
supervise, so once the files are installed the desktop integration behaves like
that of any other input method.

```
integrations/fcitx5/nexkey.cpp              the addon
integrations/fcitx5/data/nexkey-addon.conf  addon metadata, Library=nexkey
integrations/fcitx5/data/nexkey.conf        input method entry, LangCode=vi
```

## How it works

| Piece | Purpose |
| --- | --- |
| `NexKeyEngine` | the `fcitx::InputMethodEngine` implementation |
| `nexkey-state` | an `InputContextProperty` holding one `nexkey::Engine`, so every window, tab and text field keeps its own composition |
| `keyEvent` | the toggle shortcut, the word selection window, backspace and the ordinary keystroke path |
| `subMode` / `overrideIcon` | `VI` or `EN` in the panel, with the matching `nexkey-vn` / `nexkey-en` icon |
| configuration watch | the addon reloads `nexkey.conf` when it changes, at most four times per second |

The Vietnamese rules themselves live in `nexkey-core`, shared with the CLI, the
tray application and the IBus engine, so the two backends produce the same text.
[docs/ARCHITECTURE.md](ARCHITECTURE.md) has the details.

## Requirements

| What | Package (Debian / Ubuntu) |
| --- | --- |
| Fcitx5 itself | `fcitx5` |
| Build files | `libfcitx5core-dev` |
| Configuration window | `fcitx5-config-qt` (binary `fcitx5-configtool`) |
| Input in GTK / Qt applications | `fcitx5-frontend-gtk3`, `fcitx5-frontend-gtk4`, `fcitx5-frontend-qt5`, `fcitx5-frontend-qt6` |

Fcitx5 5.0 or newer is enough; the addon is built against the API that is
present in Ubuntu 24.04 (5.1.7) and does not use anything added later.

## Build and install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build --prefix /usr
```

Installed files:

```
/usr/lib/<libdir>/fcitx5/nexkey.so                     the addon
/usr/share/fcitx5/addon/nexkey-addon.conf              registration
/usr/share/fcitx5/inputmethod/nexkey.conf              input method entry
/usr/share/icons/hicolor/scalable/apps/nexkey.svg      entry icon
/usr/share/icons/hicolor/scalable/status/nexkey-*.svg  panel icons
```

The Debian package (`./scripts/package-deb.sh`) does the same and refreshes the
icon cache. Build with `-DNEXKEY_BUILD_FCITX5=OFF` to skip the addon; building
without `libfcitx5core-dev` installs everything else and prints a warning.

## Enable NexKey

1. Restart Fcitx5 so it rescans the addon and input method directories:

   ```bash
   fcitx5 -d --replace
   ```

2. Open the configuration window:

   ```bash
   fcitx5-configtool
   ```

3. On the *Input Method* tab, add **NexKey** (it shows the label `VI`), then
   move it into the active group next to *Keyboard*. Keep *Keyboard* first if
   you want plain English typing to keep working; Fcitx5 switches between the
   two with its own trigger key (usually `Control+space`).

4. Switch to NexKey with that trigger and start typing. The panel shows `VN`
   while NexKey composes Vietnamese and `EN` when the mode is off.

Verify that Fcitx5 found the addon:

```bash
fcitx5-diagnose | grep -i nexkey     # lists the addon and its library
fcitx5-remote -n                     # the input method in use, e.g. nexkey
```

## Typing

| What you type | What appears |
| --- | --- |
| `tieengs` (Telex) | `tieng` with a circumflex on the e and an acute on the first vowel, i.e. the Vietnamese word for "language" |
| `tie6ng1` (VNI) | exactly the same text, because both methods describe it |
| `hoaf` with the traditional style | the word `hoa` with a grave tone on the `o`; the candidate window also offers the modern placement (tone on the `a`) and `hoaf` |
| `windows` | `windows`, unchanged |

| Control | Effect |
| --- | --- |
| `Control+Shift` (default `ui.toggle_key`) | switch between VI and EN |
| `1` to `9` | pick an entry of the word selection window (Telex) |
| `Ctrl` plus a digit | the same in VNI and VIQR, where digits type tone markers |
| `Up` / `Down`, `Page Up` / `Page Down` | move the highlight, turn the page |
| `space` | takes the highlighted entry, then types the space itself |
| mouse click | picks the entry under the pointer |
| `Return`, `Tab`, `Escape` | commit the composition and reach the application |

Run `nexkey test "tieengs"` in a terminal to print the composed text; it uses
the same core library as the addon, so the two never disagree.

The candidate window only appears while a word has more than one reading;
`nexkey config set engine.candidates false` turns it off.

## Configure

NexKey keeps its own configuration, shared with the IBus engine, the tray
application and the CLI, instead of using the Fcitx5 configuration store. That
is why the entry is `Configurable=False` and `fcitx5-configtool` shows no
settings button.

```bash
nexkey-gui                                   # tray icon + settings window
nexkey config                                # print every effective value
nexkey config set input.method vni
nexkey config set input.tone_style traditional
nexkey config set output.encoding tcvn3
nexkey config set ui.toggle_key Ctrl+Shift+space
nexkey config set engine.candidates false
```

The file is `~/.config/nexkey/nexkey.conf`; the addon notices a change within a
quarter of a second and applies it to every open input context without a
restart. [docs/CONFIGURATION.md](CONFIGURATION.md) lists every key.

## Troubleshooting

| Symptom | Cause and fix |
| --- | --- |
| NexKey is missing from the input method list | the files are not where Fcitx5 looks for them, or Fcitx5 was not restarted: check `ls /usr/lib/<libdir>/fcitx5/nexkey.so` and run `fcitx5 -d --replace` |
| The addon appears in `fcitx5-diagnose` but typing does nothing | NexKey is in EN mode: the panel icon is `EN`, press the toggle key |
| The toggle key does nothing | it collides with the Fcitx5 trigger or a desktop shortcut; pick another combination with `nexkey config set ui.toggle_key` |
| Vietnamese text appears twice | IBus and Fcitx5 are both running; use one of them (see [docs/IBUS.md](IBUS.md)) |
| No input at all in GTK or Qt applications | install the matching `fcitx5-frontend-*` package; under X11 the environment normally needs `GTK_IM_MODULE=fcitx`, `QT_IM_MODULE=fcitx` and `XMODIFIERS=@im=fcitx` |
| No candidate window on GNOME Wayland | GNOME has no built-in panel for Fcitx5; install the *kimpanel* GNOME extension, or use the classic user interface |
| Wrong characters in old documents | the output charset is not what the document expects: `nexkey config set output.encoding tcvn3` or convert with `nexkey convert` |

`fcitx5-diagnose` prints the environment, the frontends and the loaded addons;
it is the first thing to run when something is off. `nexkey test "tieengs"`
bypasses Fcitx5 entirely and shows whether the engine itself behaves.

## Uninstall

```bash
sudo apt remove nexkey          # Debian package
# or, after a manual install:
sudo rm -rf /usr/lib/<libdir>/fcitx5/nexkey.so \
            /usr/share/fcitx5/addon/nexkey-addon.conf \
            /usr/share/fcitx5/inputmethod/nexkey.conf
fcitx5 -d --replace
```

The user configuration in `~/.config/nexkey/` stays until it is deleted by hand.
