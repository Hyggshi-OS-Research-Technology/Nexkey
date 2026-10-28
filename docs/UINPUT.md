# Deleting through a virtual keyboard

Some applications do not delete. They accept `deleteSurroundingText` and do
nothing with it, and they treat a forwarded `BackSpace` as a synthetic event
that never happened. Chrome and Chromium-based editors, some Electron
applications, LibreOffice in a few fields and most toolkits behind a remote
desktop behave this way, and for a direct-typing input method that means the
word on screen grows into nonsense the moment a tone mark is typed.

One thing every application does honour is a key press that comes from a
keyboard device. Linux can create such a device from user space: `/dev/uinput`
lets a process register a virtual keyboard and write input events, and the
kernel delivers them to whatever window has focus, exactly like the events of a
real keyboard. If NexKey presses `BACKSPACE` that way, the application deletes
the character — because as far as it can tell, someone pressed a key.

That is what `nexkey-uinput` is for. This document describes the helper, the
protocol, the bookkeeping in the input method, the permissions it needs, and
what it deliberately cannot do.

## Two processes

    Fcitx5 + NexKey addon ──── unix socket ────► nexkey-uinput ──► /dev/uinput
            (types the text)                     (presses BACKSPACE)

The addon never opens `/dev/uinput`. The helper is a separate program that owns
the device and a listening socket, and it stays alive across restarts of
Fcitx5: a virtual keyboard that disappeared with the input method would leave
applications with a device that no longer answers.

The two processes need different things and get only what they need:

| Process | Needs | Can |
| --- | --- | --- |
| addon | the socket path | ask for 0–64 `BACKSPACE` presses |
| helper | `/dev/uinput` | press `BACKSPACE` and one barrier key, nothing else |

## The protocol

One text line in each direction:

    NEXKEY-UINPUT 1 DEL <count> <pre_ms> <interval_ms> <post_ms>\n

| Field | Meaning | Limit |
| --- | --- | --- |
| `count` | how many times to press and release `BACKSPACE` | 0–64 |
| `pre_ms` | wait before the first press | 0–200 |
| `interval_ms` | wait between two presses | 0–200 |
| `post_ms` | wait between the last release and the barrier | 0–200 |

The helper answers `OK <count>` when it wrote the events, or `ERR <reason>` when
it refused. `PING` is answered with `PONG` and exists so that the addon can find
out whether a helper is there before it plans a word.

There is deliberately **no field for a key code**. The helper cannot type text,
cannot press a shortcut and cannot hold a key down: the only keys the device
registers are `KEY_BACKSPACE` and `KEY_PAUSE` (`UI_SET_KEYBIT` for exactly those
two), so even a request that a bug turned into something else would be rejected
by the device itself.

The default timing is 10 ms before the first press, 4 ms between presses and
6 ms before the barrier. The delays are not decoration: writing all the events
at once makes some toolkits coalesce or drop them, and pressing a key while
another one is still held confuses applications that track modifiers. One press
is four events: `EV_KEY 1`, `EV_SYN`, `EV_KEY 0`, `EV_SYN`.

## The barrier, and why the count is not trusted

The addon cannot know when the kernel finished delivering the burst. It cannot
read the keyboard, and an application applies its queue on its own schedule. So
the helper presses one more key after the last `BACKSPACE`: `KEY_PAUSE`, which
maps to the `Pause` keysym. That key:

- produces no text, and almost no application binds it;
- is at the same position on every keyboard layout;
- closes the burst in order. By the time it arrives, every `BACKSPACE` written
  before it has already been handed to the application.

The addon recognises `Pause`, swallows it (it never reaches the application) and
types the replacement. The count of backspaces is only a shortcut for the common
case: if the events were coalesced the count never arrives, but the barrier
still does, and the barrier is the one that matters.

## The addon side

`nexkey::ReplacementController` (`core/include/nexkey/Replacement.hpp`) is the
bookkeeping, free of any toolkit code and covered by `tests/test_replacement.cpp`:

| Situation | What happens |
| --- | --- |
| `start(0, text)` | nothing to delete, the text is typed at once |
| burst `BACKSPACE` arrives | counted and swallowed by the input method, and **passed on to the application**, because pressing it is what deletes |
| barrier arrives | burst complete, the text is typed |
| barrier arrives after the count finished | still swallowed: it was already on its way |
| a text key during the burst | buffered (up to 16) and replayed afterwards, in order |
| 16 keys already buffered | the burst ends instead of dropping the key |
| `Return`, `Tab`, arrows, a shortcut | the burst ends early (`finish_early`), the text is typed, the key is handled normally, and the remaining backspaces are still swallowed (`Draining`) |
| nothing arrives within 400 ms | `Expired`: the word is dropped, and two in a row move the program one rung down the ladder |

The text of a replacement is committed as one batch: the tail of the burst and
the characters replayed right after it leave as a single `commitString`, which
is what slow clients need. `nexkey::CommitBatcher` does the merging, and anything
that changes the document flushes first so that the batch can never reorder text
across a deletion.

The addon drives all of this from two places: the key events that arrive (the
burst's own keys are what tell it that the burst is progressing) and a 20 ms
timer, which is what finishes a burst whose barrier was lost and what types the
text when the user stops typing.

## Permissions

The device node is not writable by ordinary users by default. Two ways to give
the helper what it needs, in order of preference:

1. **A udev rule for the current seat.** `packaging/udev/60-nexkey-uinput.rules`
   grants write access to the user of the active seat only:

       sudo install -m 0644 packaging/udev/60-nexkey-uinput.rules /etc/udev/rules.d/
       sudo udevadm control --reload
       sudo udevadm trigger --name-match=uinput

2. **The `uinput` group**, where the distribution has one:

       sudo usermod -aG uinput "$USER"      # log out and back in afterwards

Some kernels do not provide `/dev/uinput` at all (`CONFIG_INPUT_UINPUT=n`), and
containers usually do not pass it through. NexKey works without it: see the
ladder below.

## Running it

    nexkey-uinput                 # the default socket
    nexkey-uinput --socket PATH   # somewhere else, for testing
    nexkey-uinput --dry-run       # accept requests, do not open a device
    nexkey-uinput --verbose       # print every request

The socket lives at `$XDG_RUNTIME_DIR/nexkey/uinput.sock`. The helper creates the
directory with mode `0700` and the socket with mode `0600`, refuses a socket path
whose directory is not owned by the current user (so `/tmp` is refused), and
removes the socket when it exits.

A systemd user unit is installed with the helper (`nexkey-uinput.service`). It is
not enabled by default, because nothing depends on it:

    systemctl --user enable --now nexkey-uinput

Removing NexKey again is `scripts/uninstall-local.sh`, which stops and disables
the unit before it deletes the binary; doing it by hand means
`systemctl --user disable --now nexkey-uinput` first, or the next install cannot
replace a helper that is still running.

`--dry-run` needs no device and is the way to try the whole path on a machine
where `/dev/uinput` is not available: start the helper with `--dry-run`, then run
the addon and watch the requests arrive in the helper's output.

## Security

What the helper can do is narrow on purpose:

- **Only the same user.** `SO_PEERCRED` gives the uid of the peer; the helper
  requires it to equal its own uid, and it refuses a root peer as well (root
  never runs an input method, so a root client is a mistake or a probe). The
  check is `nexkey::uinput::trusted_peer`, a pure function with tests.
- **Only through a private socket.** The directory is `0700` and owned by the
  user, so no other user can reach the socket in the first place; the uid check
  is the second lock, not the only one.
- **Only `BACKSPACE` and `PAUSE`.** Neither the protocol nor the device can
  produce anything else.
- **Bounded.** At most 64 presses per request, at most 200 ms per delay, and one
  request is handled at a time. It is not a way to hold a key down.
- **Write only.** A uinput device can only inject events; it cannot read the
  keyboard, the clipboard or the focused window.

The systemd unit adds the usual hardening (`NoNewPrivileges`, `ProtectSystem`,
`RestrictAddressFamilies=AF_UNIX`, `MemoryDenyWriteExecute`, ...), because the
helper needs no other part of the session.

What it cannot promise:

- A local process running as the same user can talk to the socket and make the
  kernel press `BACKSPACE` in the focused window. That is the same power the
  user already has with their own keyboard, but it is worth knowing.
- The input method and the application see two different queues, so the barrier
  guarantees *order*, not that the application repainted. A client that batches
  its own input handling can still receive the burst and the text in the same
  frame; that is why the text is typed only after the barrier and why the whole
  path can be turned off per program.
- Switching focus in the middle of a burst sends the remaining backspaces to the
  window that got focus. The window is a few tens of milliseconds long, and the
  word is dropped rather than typed into the second window.

## The delivery ladder

The helper is the top rung, not the only one. `nexkey::DeliveryPolicy` picks the
best rung an application supports:

| Rung | Deletion | Used when |
| --- | --- | --- |
| `direct-uinput` (also `direct-inject`) | the virtual keyboard, then the text after the barrier | a helper answered a probe and the client reports nothing usable — or a rule asks for it |
| `direct` | `deleteSurroundingText` | no helper, but the client reports its text |
| `direct-keys` | forwarded `BackSpace` keys | no helper and no text report |
| `preedit` | none, the word is committed once | the client dropped both, or a rule asks for it |

Two failures in a row write the next rung into `[app_rules]` for that program,
so the next session starts one step down:

    nexkey config set app_rules.chrome direct-keys   # choose a rung explicitly
    nexkey config set app_rules.chrome auto          # forget the learned rule

The tray application's diagnostics page shows the rung in use and the sentence
that explains it, for example *"Mặc định: bàn phím ảo nexkey-uinput xóa hộ"* or
*"Chưa chạy nexkey-uinput, xóa theo cách ứng dụng hiểu"*.

## Tests

Everything except the kernel round trip is tested in the ordinary build:

- `tests/test_uinput.cpp` — the protocol, its limits, `trusted_peer`, the event
  sequence, the timing plan, and a fake helper on a real socket (accepted,
  refused, missing, wedged).
- `tests/test_replacement.cpp` — the burst, the barrier, buffering, the deadline,
  the early finish, and the batcher.

The kernel side is not covered, because a test that needs `/dev/uinput` cannot
run in a container or on a CI machine. `nexkey-uinput --dry-run` exercises the
same protocol without the device, which is what the socket tests use.

## Licence and clean room

NexKey is MIT. The idea of deleting through a virtual keyboard is how the GPL-3
licensed fcitx5-lotus solves the same problem, and NexKey follows that idea
deliberately, because it is the only mechanism that works in applications which
ignore everything else.

No line of fcitx5-lotus was read, copied, transliterated or ported while this was
written. The design was taken from what the project says about itself in public
(the idea of a helper that presses `BACKSPACE` through `/dev/uinput`, of bursts
with a barrier, of keys buffered during a burst, and of a commit batcher), and
everything here — the protocol, the timing plan, the helper, the controller, the
tests, and this document — is NexKey's own. Every file in the tree carries
`SPDX-License-Identifier: MIT`.
