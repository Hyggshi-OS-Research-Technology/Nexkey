# Fcitx5 addon

NexKey speaks Fcitx5 natively: the engine is an addon that Fcitx5 loads when the
NexKey input method is selected. Once the files are installed the desktop
integration behaves like that of any other input method.

There is one optional companion process. `nexkey-uinput` deletes through a
virtual keyboard device for the applications that ignore `deleteSurroundingText`
and forwarded keys, and the addon uses it only when it is running; without it
the addon deletes the next best way and nothing else changes. It needs write
access to `/dev/uinput`, which is why it is a separate program rather than part
of the addon. [UINPUT.md](UINPUT.md) describes it, including the udev rule and
the systemd user unit.

```
integrations/fcitx5/nexkey.cpp              the addon
integrations/fcitx5/data/nexkey-addon.conf  addon metadata, installed as addon/nexkey.conf
integrations/fcitx5/data/nexkey.conf        input method entry, Addon=nexkey, LangCode=vi
```

## How it works

| Piece | Purpose |
| --- | --- |
| `NexKeyEngine` | the `fcitx::InputMethodEngine` implementation |
| `nexkey-state` | an `InputContextProperty` holding one `nexkey::Engine`, so every window, tab and text field keeps its own composition |
| `keyEvent` | the toggle shortcut, the word selection window, backspace, the keys of a deletion burst and the ordinary keystroke path |
| `ReplacementController` | per input context: the burst of deletions in flight, the keys buffered during it and the deadline that ends it |
| `timer` | one 20 ms `EventSourceTime` that finishes a burst whose barrier was lost and types the text when the user stops typing |
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
/usr/share/fcitx5/addon/nexkey.conf                    registration
/usr/share/fcitx5/inputmethod/nexkey.conf              input method entry
/usr/share/icons/hicolor/scalable/apps/nexkey.svg      entry icon
/usr/share/icons/hicolor/scalable/status/nexkey-*.svg  panel icons
```

Fcitx5 takes the name of an addon from the file name in `addon/`, and it drops
an input method entry whose `Addon=` key does not name one of the addons it
found - without printing anything. That is why the metadata file is called
`nexkey-addon.conf` in the source tree but is installed as `nexkey.conf`.

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
scripts/check-install.sh             # checks the installed files, see below
fcitx5-diagnose | grep -i nexkey     # lists the addon and its library
fcitx5-remote -n                     # the input method in use, e.g. nexkey
```

`scripts/check-install.sh [--prefix DIR]` walks the same steps Fcitx5 does:
it looks for the addon file, checks that the shared object exports the addon
factory, that the input method entry points at an addon that exists, that the
icons and the tray application are there, and whether Fcitx5 has to be
restarted. It prints one line per check and exits with 1 when something is
missing.

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

### Typing in many applications

NexKey types straight into the document, which is what makes it feel like a
keyboard, and the replacement of an earlier letter is the part applications
disagree about. Instead of a list of program names, the addon watches what each
application actually does with a replacement and steps down a ladder:

1. `direct`: type into the document, delete through `deleteSurroundingText`.
2. `direct-keys`: type into the document, delete by forwarding `BackSpace`.
   This is the path for browsers, Electron applications and anything else that
   accepts the delete request and ignores it.
3. `preedit`: show the word with an underline and commit it once, like most
   Linux input methods. Nothing is ever deleted, so every application can do it.

When the second replacement in a row is dropped, NexKey repairs the word on
screen (so the text stays what was typed), writes the next step into
`[app_rules]` and saves the configuration; the next session starts one rung
lower. The reason is shown in the tray diagnostics, together with the number of
recent mismatches:

```bash
cat ~/.cache/nexkey/status.conf
```

To force a path yourself, add a rule and restart nothing:

```bash
nexkey config set app_rules.chrome preedit
nexkey config set app_rules.gnome-text-editor direct-keys
nexkey config set app_rules.code telex+direct-keys   # method and path together
nexkey config set app_rules.chrome auto              # forget the rule
```

Deleting the line makes NexKey try the faster path again.

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
| NexKey is missing from the input method list | run `scripts/check-install.sh`: it names the missing or mismatched file. Usually the files are not installed, or Fcitx5 was started before them - `fcitx5 -d --replace` fixes the second case |
| NexKey stays missing although the files are there | the addon registration and the input method entry disagree. Fcitx5 takes the addon name from the file name, so `share/fcitx5/addon/nexkey.conf` has to exist next to `inputmethod/nexkey.conf`; the checker reports this case and names the file that is wrong |
| an old NexKey build left files behind | remove the leftovers the checker lists, for example `/usr/share/fcitx5/addon/nexkey-addon.conf`, then restart Fcitx5 |
| The addon appears in `fcitx5-diagnose` but typing does nothing | NexKey is in EN mode: the panel icon is `EN`, press the toggle key |
| The toggle key does nothing | it collides with the Fcitx5 trigger or a desktop shortcut; pick another combination with `nexkey config set ui.toggle_key` |
| Vietnamese text appears twice | IBus and Fcitx5 are both running; use one of them (see [docs/IBUS.md](IBUS.md)) |
| A word loses a letter in one application only | the application ignores `deleteSurroundingText`; NexKey repairs the word and switches that program to `direct-keys` after the second occurrence. Add `nexkey config set app_rules.<program> preedit` to go straight to the underlined path |
| The underscore appears in a terminal or a chat window | that application has no client preedit, so the word is drawn in the input method panel; the text itself is correct. `nexkey config set engine.direct_typing false` makes it the same everywhere |
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
            /usr/share/fcitx5/addon/nexkey.conf \
            /usr/share/fcitx5/inputmethod/nexkey.conf
fcitx5 -d --replace
```

The user configuration in `~/.config/nexkey/` stays until it is deleted by hand.
