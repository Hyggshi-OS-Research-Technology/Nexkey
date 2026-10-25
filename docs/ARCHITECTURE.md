# Architecture

NexKey is split into a pure text engine and thin frontends. The engine has no
dependency outside the C++23 standard library, which keeps it easy to test and
easy to reuse.

```
nexkey-core        core/, C++23, no dependencies
nexkey (CLI)       cli/
nexkey-gui         gui/, Qt6 Widgets and DBus
nexkey (addon)     integrations/fcitx5/, Fcitx5
nexkey-tests       tests/
```

## Core

| Header | Responsibility |
| --- | --- |
| `nexkey/Types.hpp` | Enumerations, `Options`, name lookup for methods and charsets |
| `nexkey/Grapheme.hpp` | Letter model, Unicode composition and decomposition tables |
| `nexkey/Engine.hpp` | Keystroke state machine (`type`, `backspace`, `flush`) |
| `nexkey/Converter.hpp` | Conversion between Unicode, NFD, TCVN3 and VIQR |
| `nexkey/Settings.hpp` | INI configuration reader and writer |
| `nexkey/Unicode.hpp` | UTF-8 and UTF-32 helpers |

Internal translation units:

| File | Responsibility |
| --- | --- |
| `core/src/Word.cpp` | Keystroke parsing per input method, word composition |
| `core/src/Syllable.cpp` | Onset, nucleus and coda tables, tone position, plausibility |
| `core/src/Grapheme.cpp` | Composed code point table and reverse lookup |

### From keystrokes to text

1. `Engine` stores the raw ASCII keystrokes of the word being typed.
2. `detail::parse` replays the keystrokes with the rules of the active input
   method. It recognises tone keys, doubled letters, the `w` and bracket
   shortcuts (Telex), digits (VNI) and punctuation markers (VIQR). Every
   modifier records the keystroke that applied it, so pressing the same key
   again takes the modifier back and types the key literally.
3. `detail::analyse` splits the resulting letters into onset, nucleus and coda
   using tables of valid Vietnamese phonemes. The same tables decide whether a
   partially typed word can still become a syllable.
4. `detail::tone_index` chooses the letter that carries the tone mark:
   - a letter with a vowel modifier wins (the last one in `uo` with horns);
   - a triphthong takes the tone on the middle vowel;
   - a closed syllable takes it on the second vowel of the nucleus;
   - an open syllable follows the configured style for `oa`, `oe`, `uy`.
5. `detail::compose_word` writes the precomposed Unicode string, and
   `Converter::encode` turns it into the requested charset.
6. When the composed word is not a plausible Vietnamese word and
   `english_fallback` is enabled, the engine returns the raw keystrokes
   instead, so English words are never mangled.

### Engine state

`Engine` keeps three things: the raw keystrokes, the rendered preedit and a
history stack of raw keystrokes used by the smart backspace. Word boundaries
(space, punctuation) commit the preedit plus the boundary character and start a
new word. `flush` commits the pending composition, `discard` drops it.

## Frontends

### Fcitx5 addon

`integrations/fcitx5/nexkey.cpp` implements `fcitx::InputMethodEngine`. Key
events are translated into Unicode code points with `fcitx::Key::keySymToUnicode`
and forwarded to one `nexkey::Engine` instance per input context. The preedit is
published through the client preedit, commits go through
`InputContext::commitString`, and `subMode`/`overrideIcon` provide the VI/EN
state to the panel.

The addon watches the modification time of the configuration file and reloads
it lazily, at most four times per second, so changes made by the tray
application apply without restarting Fcitx5.

### Tray application

`gui/Daemon.cpp` owns the configuration and exposes it on the session bus.
`gui/TrayApplication.cpp` draws the tray icon at runtime, so no icon theme is
required, and `gui/SettingsDialog.cpp` edits the configuration file. A single
instance is enforced with `registerService("org.nexkey.Daemon")`; a second
start asks the running instance to open its settings window.

### Command line tool

`cli/` contains the option parser, the command implementations and the help
screens. It is a thin layer over `nexkey-core` and needs no display or session
bus.

## Testing

`tests/TestHarness.hpp` provides a minimal `TEST` macro with automatic
registration and source locations in failure messages. The tests exercise the
public API of the core, plus the internal syllable helpers, so a regression in
tone placement fails immediately.

## Charsets

- **Unicode** is precomposed UTF-8, the format every modern application expects.
- **Unicode decomposed** stores base letters plus combining marks, which is
  what some older transfer formats and normalisation sensitive tools expect.
- **TCVN3** is a single byte legacy charset from the DOS era. Vietnamese
  letters live in the `0xA1` to `0xFE` range and the case of an accented letter
  is not stored.
- **VIQR** is pure ASCII: `a^`, `a(`, `o+`, `u+`, `d-` and a tone marker chosen
  from `'`, `` ` ``, `?`, `~`, `.` placed after the vowel cluster.

Encoding and decoding are exact inverses for the Vietnamese alphabet, which is
covered by round trip tests.

In VIQR mode the tone markers are part of the word, so a literal `.` or `?`
typed right after a vowel has to be pressed twice: the first press applies the
tone, the second press takes it back and types the character.
