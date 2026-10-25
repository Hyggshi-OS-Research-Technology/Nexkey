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

### `[ui]`

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `toggle_key` | portable key name | `Control+Shift` | Shortcut that switches between VI and EN |
| `show_tray_icon` | `true`, `false` | `true` | Show the tray icon |
| `start_with_system` | `true`, `false` | `false` | Install an XDG autostart entry |

`toggle_key` accepts a modifier only combination such as `Control+Shift`, which
triggers when the modifiers are released, or a full combination such as
`Ctrl+Shift+space` / `Control+space`, which triggers on the key press.

## Command line

```bash
nexkey config                 # print every effective value
nexkey config get input.method
nexkey config set input.method vni
nexkey config set engine.english_fallback false
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
| `ModeChanged(QString)` | signal | Emitted when the mode changes |
| `SettingsChanged()` | signal | Emitted when any setting changes |
| `SettingsRequested()` | signal | Emitted when a window should be shown |

Examples:

```bash
qdbus org.nexkey.Daemon /org/nexkey/Daemon Toggle
busctl --user call org.nexkey.Daemon /org/nexkey/Daemon org.nexkey.Daemon Toggle
```

Starting `nexkey-gui` a second time forwards the request to the running
instance and opens its settings window.

## Fcitx5

The addon rereads the configuration file when it changes, so no restart is
needed after `nexkey config set`. The mode may also be switched with the
configured toggle key, from the tray icon or over D-Bus.
