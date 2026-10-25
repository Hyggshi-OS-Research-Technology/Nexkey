# Configuration

NexKey keeps one configuration file per user:

```
$XDG_CONFIG_HOME/nexkey/nexkey.conf      # usually ~/.config/nexkey/nexkey.conf
```

The file is plain INI text. Missing keys keep their default value, unknown
sections and unknown keys are ignored, and a broken value never stops NexKey
from starting. The tray application, the Fcitx5 addon and the CLI all read the
same file, so a change made in one place is visible everywhere.

## File format

```ini
# comments start with '#' or ';'
[input]
method=telex
tone_style=modern
enabled=true

[output]
encoding=unicode

[engine]
smart_backspace=true
english_fallback=true
bracket_shortcuts=true

[ui]
toggle_key=Control+Shift
show_tray_icon=true
start_with_system=false
```

## Keys

### `[input]`

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `method` | `telex`, `telex-simple`, `vni`, `viqr` | `telex` | Key mapping used to compose words |
| `tone_style` | `modern`, `traditional` | `modern` | Tone position in open syllables with an `o` or `u` glide |
| `enabled` | `true`, `false` | `true` | Vietnamese mode (`VI`) or English mode (`EN`) |

`telex-simple` keeps the tone keys and the doubled letters but drops the `w`
shortcut and the bracket shortcuts.

### `[output]`

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `encoding` | `unicode`, `unicode-decomposed`, `tcvn3`, `viqr` | `unicode` | Character set of the committed text |

### `[engine]`

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `smart_backspace` | `true`, `false` | `true` | Skip keystrokes that had no visible effect when deleting |
| `english_fallback` | `true`, `false` | `true` | Show the raw keystrokes while the word is not Vietnamese |
| `bracket_shortcuts` | `true`, `false` | `true` | `[`, `]`, `{`, `}` produce the horned vowels in Telex |
| `candidates` | `true`, `false` | `true` | Show the word selection window with the alternative readings |

### `[ui]`

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `toggle_key` | portable key name | `Control+Shift` | Shortcut that switches between VI and EN |
| `show_tray_icon` | `true`, `false` | `true` | Show the tray icon |
| `start_with_system` | `true`, `false` | `true` | Start the tray icon with the desktop session |

`toggle_key` accepts a modifier only combination such as `Control+Shift`, which
triggers when the modifiers are released, or a full combination such as
`Ctrl+Shift+space` / `Control+space`, which triggers on the key press.

`start_with_system` is applied to the standard XDG autostart directory. A
distribution package installs `/etc/xdg/autostart/nexkey.desktop`; switching the
option off writes `~/.config/autostart/nexkey.desktop` with `Hidden=true`, which
hides that entry for the current user only. Without a package, for example with
the AppImage, a normal per user entry is written instead.

## Candidate window

While a word is being composed both backends show the alternative readings of
the keystrokes, so the user can pick one instead of relying on the default:

| Entry | Meaning |
| --- | --- |
| the preedit itself | what the current settings produce, the default |
| the other tone style | same word with `modern` or `traditional` placement, commented with the style it uses |
| the raw keystrokes | the keys exactly as typed, commented `raw`, useful when a rule mangles an unusual word |

Selection keys:

- Telex and Simple Telex: `1` to `9` pick an entry, because digits have no
  meaning in those methods.
- VNI and VIQR: digits are tone markers and literal text, so they keep typing
  and selection uses `Ctrl` plus a digit.
- `Up` and `Down` move the highlight, `Page Up` and `Page Down` turn the page,
  and `space` takes the highlighted entry and then types the space itself, so a
  word is never followed by a missing separator.
- A click in the candidate window picks the entry under the pointer.

The keys that belong to the window are consumed; everything else, including
`Return`, `Tab` and `Escape`, ends the composition and reaches the application.
Each window, tab and text field keeps its own composition, so switching between
two applications never mixes two half typed words.

The window only appears when there is something to choose; a word with a single
possible reading is committed normally. Turn it off with
`nexkey config set engine.candidates false`.

## Command line

```bash
nexkey config                 # print every effective value
nexkey config get input.method
nexkey config set input.method vni
nexkey config set engine.english_fallback false
nexkey config set engine.candidates false
nexkey config path            # print the path of the configuration file
nexkey status                 # same values, plus the current mode
nexkey toggle                 # switch between VI and EN mode
```

## D-Bus interface

The tray application registers `org.nexkey.Daemon` on the session bus at
`/org/nexkey/Daemon`:

| Member | Kind | Description |
| --- | --- | --- |
| `Mode` | property (read) | `VI` or `EN` |
| `Enabled` | property (read, write) | Same information as a boolean |
| `Method` | property (read) | Active input method |
| `Encoding` | property (read) | Active charset |
| `GetMode()` | method | Returns `VI` or `EN` |
| `GetEnabled()` | method | Returns the mode as a boolean |
| `SetEnabled(bool)` | method | Switches the mode |
| `Toggle()` | method | Switches the mode |
| `Reload()` | method | Re-reads the configuration file |
| `ShowSettings()` | method | Opens the settings window of the running instance |
| `SettingsRequested()` | signal | Emitted when a window should be shown |
| `ModeChanged(QString)` | signal | Emitted when the mode changes |
| `SettingsChanged()` | signal | Emitted when any setting changes |

Examples:

```bash
qdbus org.nexkey.Daemon /org/nexkey/Daemon Toggle
busctl --user call org.nexkey.Daemon /org/nexkey/Daemon org.nexkey.Daemon Toggle
```

Starting `nexkey-gui` a second time forwards the request to the running
instance and opens its settings window.

## Fcitx5 and IBus

Both backends reread the configuration file when it changes, so no restart is
needed after `nexkey config set`. The mode may also be switched with the
configured toggle key, from the tray icon, over D-Bus or from the menu item of
the input method panel.

The IBus engine is documented separately in [IBUS.md](IBUS.md).
