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
| `nexkey/Unicode.hpp` | Strict UTF-8, code point and grapheme cluster measuring |

Internal translation units:

| File | Responsibility |
| --- | --- |
| `core/src/Word.cpp` | Keystroke parsing per input method, word composition |
| `core/src/Syllable.cpp` | Onset, nucleus and coda tables, tone position, plausibility |
| `core/src/Grapheme.cpp` | Composed code point table and reverse lookup |

### Text measurement

`nexkey/Unicode.hpp` is the only place where NexKey interprets bytes as text,
and it follows the well formedness rules of the Unicode Standard instead of the
"first byte tells the length" shortcut. Overlong forms, UTF-16 surrogates and
code points above U+10FFFF are not text: `decode` replaces each maximal subpart
with one U+FFFD and `append` never writes a byte sequence a strict decoder would
reject. The frontends rely on this, because they compare the document with what
they wrote and a lenient decoder would hide corruption instead of showing it.

The measuring helpers sit on top of the decoder, and they are what the direct
typing path of the frontends needs:

| Helper | Answer |
| --- | --- |
| `count_code_points` | code points, the unit the surrounding text protocol counts |
| `common_prefix` | bytes, code points and clusters shared by two strings |
| `count_graphemes`, `grapheme_boundary` | visible characters, marks included |

`common_prefix` gives the cut between the part of a word a frontend has to
delete and the part it commits, and it never lands inside a code point.
`grapheme_boundary` makes sure that deleting a letter of an NFD word takes its
combining marks with it instead of leaving a floating accent behind.

A cluster is a base code point plus the combining marks, variation selectors
and emoji modifiers that follow it. The rules that need full Unicode tables
(CR LF, the emoji ZWJ sequences) are treated as separate characters.

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
and forwarded to one `nexkey::Engine` instance per input context.

Fcitx 5 keeps several input contexts alive at once (one per window, tab or text
field) and switches between them without notice, so the composition lives in an
`fcitx::InputContextProperty` registered as `nexkey-state` rather than in the
addon: a half typed word can never leak into another window. The shared
configuration stays in the addon and carries a generation counter; an input
context picks up new options the next time it is used, and a mode change
discards the compositions of all open contexts.

The preedit is published through the client preedit, commits go through
`InputContext::commitString`, and `subMode`/`overrideIcon` provide the VI/EN
state to the panel. The word selection window is an ordinary
`fcitx::CommonCandidateList`, so the frontend draws it and the user can click an
entry; the addon intercepts the keys that belong to the list (digits,
Up/Down, PageUp/PageDown and space) and hands everything else to the word
parser.

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
