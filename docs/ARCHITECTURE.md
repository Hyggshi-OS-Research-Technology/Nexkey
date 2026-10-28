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
| `nexkey/Delivery.hpp` | Direct typing policy, edit planning and document verification |
| `nexkey/Uinput.hpp` | Wire format and timing plan of the virtual keyboard helper |
| `nexkey/Replacement.hpp` | One delete-then-type transaction and the commit batcher |
| `nexkey/UinputClient.hpp` | Client side of the helper's socket |

Internal translation units:

| File | Responsibility |
| --- | --- |
| `core/src/Word.cpp` | Keystroke parsing per input method, word composition |
| `core/src/Syllable.cpp` | Onset, nucleus and coda tables, tone position, plausibility |
| `core/src/Grapheme.cpp` | Composed code point table and reverse lookup |
| `core/src/Delivery.cpp` | Delivery ladder, `plan_edit` and `DirectSession` |
| `core/src/Uinput.cpp` | Protocol, limits, trusted peer check and timing plan of the virtual keyboard helper |
| `core/src/Replacement.cpp` | One delete-then-type transaction: burst, barrier, deadline, and commit batching |
| `core/src/UinputClient.cpp` | Non-blocking client for the helper's socket |

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
and emoji modifiers that follow it.

### Unicode backend

Everything that needs the Unicode character database (normalization, grapheme
clusters and case mapping) goes through one seam in `core/src/Unicode.cpp`:

| Backend | Selected by | Coverage |
| --- | --- | --- |
| ICU | `NEXKEY_USE_ICU=ON` and ICU 67+ found at configure time | every script, UAX #15 and UAX #29 as published |
| built-in | otherwise | the Vietnamese alphabet and the combining marks it uses |

The built-in backend exists so that build dependencies stay optional: it knows
the Vietnamese letters and marks by table and treats anything else as a base
character, which is correct for Vietnamese text and wrong for, say, emoji ZWJ
sequences or Hangul. `nexkey::unicode::has_full_unicode()` reports which one is
running, `backend_name()` returns `"icu 72.1"` or `"builtin"`, and both the CLI
(`nexkey version`) and the tray diagnostics print it, so a bug report says which
code path produced the text.

Case mapping is deliberately done in the root locale: `upper("vi\u1EC7t nam")`
gives `VI\u1EC6T NAM` and `upper("i")` gives `I` rather than the Turkish dotted
capital I. The user's locale is about the document, not about the shape of a
macro expansion, and a Turkish locale must not change how a Vietnamese macro is
capitalized.

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
   instead.

### English text

The fallback of step 6 is structural: `detail::analyse` only knows which letter
sequences can form a Vietnamese syllable. That is enough for words that cannot
be Vietnamese at all (`windows`, `jazz`, `https`, `git` - `f`, `j`, `w` and `z`
are not onsets, `nd`, `rk` and `ss` are not codas) and it is not enough for
words that happen to spell a valid syllable: typing `test` produces `tét`,
`last` produces `lát` and `car` produces `cả`, because the tone key is a letter
of an otherwise valid word. `tests/test_telex.cpp` pins both lists, so the
behaviour cannot drift unnoticed.

Separating those words needs a dictionary, and the engine cannot guess which of
two valid readings a user meant. `Engine::set_custom_words` takes the words of
the `[custom_dictionary]` section, and a word from that list is committed exactly
as typed: `Options::custom_dict` switches the list on, the frontends hand it over
on every configuration reload, and `nexkey test` uses it as well. So a user who
types `test` and `docs` all day writes those words down once and keeps the
Vietnamese rules for everything else, including `cả`, which is spelled `car`.

`spell_check` is still reserved: it is stored, exported and shown in the tray,
but the structural check runs whenever `english_fallback` is on. The dictionary
it describes needs a Vietnamese word list, which is a separate change.

### Engine state

`Engine` keeps three things: the raw keystrokes, the rendered preedit and a
history stack of raw keystrokes used by the smart backspace. Word boundaries
(space, punctuation) commit the preedit plus the boundary character and start a
new word. `flush` commits the pending composition, `discard` drops it.

### Normalization

The engine composes while it types, so the preedit and the committed text are
NFC: one code point per Vietnamese letter. Input that arrives in NFD is
understood as well, because the parser decodes a mark either way, so pasting
`e` followed by U+0302 and U+0301 behaves exactly like typing `ee'`.

The `unicode-decomposed` charset writes the other form. Its marks are sorted by
combining class, the rule of UAX #15: the horn (216) comes before the dot below
(220), which comes before the marks that sit above the letter (230). Writing
them in the order they were typed looks right but is not NFD, and a client that
normalizes its document would then disagree with NexKey about the same word.

`Converter::normalize(text, Normalization::Nfc)` and the `Nfd` counterpart
rewrite a string in either form, and `Converter::equal_normalized(a, b)` compares
two spellings of one word. Both touch only the letters of the Vietnamese
alphabet: a string without a Vietnamese letter in it comes back unchanged, so
the engine never rewrites text it does not understand.

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

#### Direct typing and the delivery ladder

Direct typing types straight into the document, so every keystroke that changes
an earlier letter is a replacement: delete the characters from the shared prefix
on, then type the new ones. `nexkey::plan_edit` computes that pair and
`nexkey::DirectSession` remembers what the document should look like. Both live
in `core/src/Delivery.cpp` and are covered by tests, because the same two
questions come up in every frontend: what do I delete, and did it work?

What the application does with the request is the interesting part. Some
toolkits accept `deleteSurroundingText` and ignore it, and a client that does not
report its text cannot be checked at all. NexKey answers with a ladder, decided
per application by `nexkey::DeliveryPolicy`:

| Rung | Deletion | Used when |
| --- | --- | --- |
| `direct-uinput` | `BACKSPACE` on a virtual keyboard device, through the kernel | `nexkey-uinput` is running, see [UINPUT.md](UINPUT.md) |
| `direct` | `deleteSurroundingText`, counted in characters | no helper, and the client reports its text |
| `direct-keys` | one forwarded `BackSpace` per character | no helper, and the client reports no text, or dropped a deletion |
| `preedit` | none, the word is committed once when it ends | the client dropped both |

Nothing in that table is a list of program names. A rung is left behind on
evidence: after each keystroke the addon compares the end of the document, as
the client reports it, with what the edit asked for (`DirectSession::verify`).

The top rung is asynchronous and cannot be checked that way, so it has its own
bookkeeping. `nexkey::ReplacementController` submits a burst, swallows the
backspaces as they come back through the input method, waits for the barrier key
that closes the burst, buffers whatever the user types meanwhile, and gives up
after a deadline. `nexkey::CommitBatcher` merges the commits of one key event so
that a slow client sees one insertion rather than three. Both are toolkit free and
tested; `nexkey-uinput`, the helper that owns `/dev/uinput`, is a separate
process with a protocol that can only ask for `BACKSPACE`. [UINPUT.md](UINPUT.md)
has the details, including what the design cannot promise.

- **Landed**: the document ends with the new text, nothing happens.
- **Deletion ignored**: the old characters are still there and the new ones
  follow them. The addon first repairs the word, deleting with forwarded
  `BackSpace` keys, because the request that just failed cannot fix anything, so
  the user keeps what they typed. Then it counts the failure: two in a row write
  the next rung into `[app_rules]` and save the configuration, so the next
  session starts one step down.
- **Not reported yet**: the client still shows the text from before the edit.
  Doing nothing is the only safe answer, because deleting again would take a
  second character with it.
- **Unrelated**: the document is not the one this word was written into, which
  means the cursor moved or another program typed there. The half typed word is
  dropped instead of writing on top of unknown text.

The check runs at the start of every keystroke, against the document as the
client reports it with that key, so every edit is checked except the one that
ends the word. A pure deletion leaves nothing to look for and is skipped the
same way: the edit that follows it types a character, and that one can be
checked.

A rung change, a settings change or a mode switch while a word is being typed is
handled in one place: the state remembers the mode it was used with, and a
different one drops the composition instead of carrying the word from the
document into the preedit or the other way round.

The IBus engine has no direct path at all. It publishes the preedit and commits
the word once, which is the last rung and the one that works everywhere; the
`direct_typing` option therefore only affects the Fcitx5 addon.

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

`tests/test_vietnamese_words.cpp` keeps a corpus of Vietnamese words with the
Telex and VNI keystrokes that produce them. Besides the expected text it checks
three properties for every word: the state after each keystroke equals rendering
that prefix from scratch (a keystroke may not depend on anything older than the
word itself), Backspace walks those states in reverse (the raw keystrokes are
kept, so undo is exact), and every commit is well formed UTF-8 in both Unicode
forms. `tests/test_unicode.cpp` adds property tests for the byte level helpers,
and the decoder agrees with the reference implementation of the Unicode
standard on a few thousand malformed byte strings. `tests/test_delivery.cpp`
covers the delivery ladder and the direct typing edits: what a replacement
deletes, what a client that ignored it looks like, and which rule NexKey learns
from that.

## Charsets

- **Unicode** is precomposed UTF-8, the format every modern application expects.
- **Unicode decomposed** stores base letters plus combining marks in canonical
  order, which is what some older transfer formats and normalisation sensitive
  tools expect. Canonical order means the marks are sorted by combining class
  (UAX #15), not by the order in which they were typed: the dot below of `ộ`
  comes before its circumflex. `Converter::normalize` rewrites a string in
  either form and `Converter::equal_normalized` compares two strings that may
  use different forms, which is what a frontend needs when it compares the
  document with what it wrote. Both delegate to the Unicode backend, so a build
  with ICU normalizes every script and a build without it normalizes the
  Vietnamese alphabet.
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
