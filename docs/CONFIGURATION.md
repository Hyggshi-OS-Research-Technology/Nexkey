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
candidates=true
direct_typing=true
spell_check=true
macro_enabled=true
capitalize_macro=true
custom_dict=false

[macro]
time_format=%H:%M:%S
date_format=%d/%m/%Y

[keymap]
preset=telex

[app_rules]
code=telex
gnome-terminal=off

[ui]
toggle_key=Control+Shift
show_tray_icon=true
start_with_system=true
```

## Keys

### `[input]`

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `method` | `telex`, `telex-simple`, `vni`, `viqr`, `microsoft` | `telex` | Key mapping used to compose words |
| `tone_style` | `modern`, `traditional` | `modern` | Tone position in open syllables with an `o` or `u` glide |
| `enabled` | `true`, `false` | `true` | Vietnamese mode (`VI`) or English mode (`EN`) |

`telex-simple` keeps the tone keys and the doubled letters but drops the `w`
shortcut and the bracket shortcuts. `microsoft` layout uses the Windows Telex preset.

### `[output]`

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `encoding` | `unicode`, `unicode-decomposed`, `tcvn3`, `viqr` | `unicode` | Character set of the committed text |

### `[engine]`

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `direct_typing` | `true`, `false` | `true` | Direct typing without preedit underlines (Non-preedit); an application that drops a replacement is moved to the next safer path automatically |
| `smart_backspace` | `true`, `false` | `true` | Skip keystrokes that had no visible effect when deleting |
| `english_fallback` | `true`, `false` | `true` | Auto-restore raw keystrokes when typing non-Vietnamese words |
| `spell_check` | `true`, `false` | `true` | Reserved for the dictionary-based spell check; stored and exported, but not consulted by the engine yet |
| `macro_enabled` | `true`, `false` | `true` | Enable macro shorthand expansion |
| `capitalize_macro` | `true`, `false` | `true` | Auto-capitalize macro expansions when abbreviation is capitalized |
| `custom_dict` | `true`, `false` | `false` | Use the `[custom_dictionary]` words: they are committed exactly as typed |
| `bracket_shortcuts` | `true`, `false` | `true` | `[`, `]`, `{`, `}` produce the horned vowels in Telex |
| `candidates` | `true`, `false` | `true` | Show the word selection window with the alternative readings |

### `[macro]`

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `time_format` | strftime format | `%H:%M:%S` | Format string for dynamic `$TIME` placeholder |
| `date_format` | strftime format | `%d/%m/%Y` | Format string for dynamic `$DATE` placeholder |

### `[keymap]`

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `preset` | `telex`, `vni`, `viqr`, `microsoft` | `telex` | Default keyboard preset layout |

### `[app_rules]`

Per-application rules, keyed by the process name of the program. A value can
name an input method, a delivery mode, or both joined with `+` or `,`
(`code=telex+preedit`), and each half is honoured independently:

| Value | Effect |
| --- | --- |
| `telex`, `telex-simple`, `vni`, `viqr`, `microsoft` | input method for that program |
| `off` (or `en`, `english`) | the program gets the keystrokes untouched |
| `direct-uinput` (also `direct-inject`) | type straight into the document, delete through the virtual keyboard helper `nexkey-uinput`; works in programs that ignore every other deletion, see [UINPUT.md](UINPUT.md) |
| `direct` | type straight into the document, delete with `deleteSurroundingText` when the program reports its text |
| `direct-keys` | type straight into the document, delete with forwarded `BackSpace` keys; slower, but programs that ignore `deleteSurroundingText` still work |
| `preedit` | show the word with an underline and commit it once; the slowest and the most compatible |

```ini
[app_rules]
code=telex
gnome-terminal=off
firefox=vni
chrome=preedit
libreoffice=telex+direct-keys
chrome=direct-uinput
```

NexKey writes a rule itself when it catches an application dropping a
replacement: the word on screen is repaired first, and the second failure in a
row saves the next step of the ladder (`direct-uinput` to `direct` to `direct-keys`
to `preedit`)
here. Only the delivery half is rewritten, so a method that was set for the same
program survives. Deleting the line, or `nexkey config set app_rules.<program>
auto`, makes NexKey try the faster path again.

### `[custom_dictionary]`

Personal custom dictionary words, one per line. A word from this list is
committed exactly as typed, so `test`, `docs` or `build` stop turning into
Vietnamese approximations, while `car` still gives `cả` unless it is listed
too. The list takes effect when `custom_dict=true`; the lookup ignores ASCII
case. See [ARCHITECTURE.md](ARCHITECTURE.md#english-text).

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
nexkey config set app_rules.chrome preedit     # one program, see [app_rules]
nexkey config set app_rules.code direct-keys   # or back to "auto" to forget it
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

## Backup & Restore (JSON)

NexKey supports exporting and importing the entire configuration using JSON files. The backup file contains 5 components that can be selectively exported and imported:

- `main_settings`: Engine options, input method, output encoding, toggle key, time/date formats.
- `macros`: Shorthand expansion dictionary and emoji shortcodes (including `$TIME`, `$DATE`).
- `custom_keymaps`: Keyboard layout preset (`telex`, `vni`, `viqr`, `microsoft`).
- `app_rules`: Per-application input mode rules.
- `custom_dictionary`: Personal custom dictionary entries.

Example `nexkey_backup.json`:

```json
{
  "version": "2.0.0",
  "main_settings": {
    "method": "telex",
    "tone_style": "modern",
    "encoding": "unicode",
    "enabled": true,
    "direct_typing": true,
    "spell_check": true,
    "macro_enabled": true,
    "capitalize_macro": true,
    "custom_dict": false,
    "toggle_key": "Control+Shift",
    "time_format": "%H:%M:%S",
    "date_format": "%d/%m/%Y"
  },
  "macros": {
    "vn": "Việt Nam",
    "kg": "khô gà $TIME",
    ":fire:": "🔥"
  },
  "custom_keymaps": {
    "preset": "telex"
  },
  "app_rules": {
    "code": "telex",
    "gnome-terminal": "off"
  },
  "custom_dictionary": [
    "kubernetes"
  ]
}
```
