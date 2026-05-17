```                         _
  ___ _ __ ___   ___  _ __ | | _____ _   _
 / __| '_ ` _ \ / _ \| '_ \| |/ / _ \ | | |
| (__| | | | | | (_) | | | |   <  __/ |_| |
 \___|_| |_| |_|\___/|_| |_|_|\_\___|\__, |
                                       |__/
```

> A terminal typing speed trainer written in C.  
> No internet. No browser. No dependencies. Pure speed.

---

## Table of Contents

- [Features](#features)
- [Demo](#demo)
- [Installation](#installation)
- [Usage](#usage)
- [Metrics](#metrics)
- [Configuration](#configuration)
- [Project Structure](#project-structure)
- [Building from Source](#building-from-source)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)

---

## Features

- **Offline-first** — all word lists are bundled into the binary; no network call ever made
- **Game-style launch** — starts on a main menu screen before entering a run
- **Real-time feedback** — correct characters shown in white, errors in red, as you type
- **Multiple modes** — timed test, word-count test, and quote mode
- **Full metrics** — WPM (net & raw), accuracy, error count, consistency, and elapsed time
- **Zero dependencies** — only libc + POSIX termios; no ncurses, no SDL, nothing else
- **Tiny footprint** — binary under 50 KB stripped
- **Portable** — builds on Linux and macOS with `gcc` or `clang`

---

## Demo

Main menu shown on startup:

```
 cmonkey

 Main Menu

 [1] Regular run
 [q] Quit
```

Typing screen during a run:

```
┌─────────────────────────────────────────────────────────────┐
│                         cmonkey                             │
├─────────────────────────────────────────────────────────────┤
│  the quick brown fox jumps over the lazy dog and then       │
│  runs away into the forest never to be seen again today     │
│                                                             │
│  > the quick brow█                                          │
│                                                             │
│  Time: 00:42  WPM: 74  ACC: 97.3%  ████████░░░░  60s       │
└─────────────────────────────────────────────────────────────┘
```

Results screen after the test:

```
  ── Results ──────────────────────────────────────────────
  WPM (net)       74
  WPM (raw)       79
  Accuracy        97.3 %
  Correct chars   386
  Errors           11
  Consistency     91.4 %
  Time             60 s
  ─────────────────────────────────────────────────────────
  [Enter] retry   [r] new words   [q] quit
```

---

## Installation

### Pre-built binary (Linux x86-64)

```bash
curl -Lo cmonkey https://github.com/yourname/cmonkey/releases/latest/download/cmonkey-linux-x86_64
chmod +x cmonkey
sudo mv cmonkey /usr/local/bin/
```

### Build from source

See [Building from Source](#building-from-source) below.

---

## Usage

```
Usage: cmonkey [OPTIONS]

Options:
  -t <seconds>      Time mode — type for N seconds (default: 60)
  -w <words>        Word mode — type exactly N words
  -q                Quote mode — type a random passage
  --list <name>     Word list to use: easy, common, full (default: common)
  --no-color        Disable ANSI color output
  -h, --help        Show this help message
  -v, --version     Print version and exit
```

### Examples

```bash
cmonkey                  # opens main menu, choose [1] for regular run
cmonkey -t 30            # 30-second sprint
cmonkey -t 120           # 2-minute endurance run
cmonkey -w 50            # type exactly 50 words, time recorded
cmonkey -q               # random quote mode
cmonkey --list easy      # beginner-friendly short words
cmonkey --no-color       # for terminals without color support
```

### Controls during a test

|Key|Action|
|---|---|
|Any letter|Type the character|
|`Backspace`|Delete the last character|
|`Escape`|Abort the current test|
|`Ctrl+C`|Quit (terminal is restored)|

### Main menu controls

|Key|Action|
|---|---|
|`1` or `Enter`|Start regular run|
|`q`|Quit|

---

## Metrics

|Metric|Description|
|---|---|
|**WPM (net)**|`(correct_chars / 5 / elapsed_s) * 60` — penalises uncorrected errors|
|**WPM (raw)**|`(total_keystrokes / 5 / elapsed_s) * 60` — no penalty, true typing speed|
|**Accuracy**|`(correct_chars / total_keystrokes) * 100`|
|**Errors**|Count of characters typed incorrectly and not backspaced over|
|**Consistency**|Coefficient of variation of per-word WPM, inverted to a 0–100 % scale|
|**Time**|Wall-clock elapsed from first keystroke to last character|

> One "word" = 5 characters (including spaces), following the standard typing test convention.

---

## Configuration

cmonkey reads `~/.config/cmonkey/config` on startup. The file is created with defaults on first run.

```ini
# ~/.config/cmonkey/config

default_time   = 60        # default seconds for timed mode
default_words  = 25        # default word count for word mode
word_list      = common    # easy | common | full
color          = true      # enable ANSI colors
```

Command-line flags always override config file values.

---

## Project Structure

```
cmonkey/
├── src/
│   ├── main.c          Entry point, argument parsing, main event loop
│   ├── terminal.c/.h   Raw-mode setup, ANSI rendering, screen clear
│   ├── words.c/.h      Word list loading and random word generation
│   ├── input.c/.h      Non-blocking keystroke reader, backspace handling
│   ├── engine.c/.h     Test state machine (idle → running → finished)
│   ├── metrics.c/.h    WPM, accuracy, and consistency calculations
│   └── config.c/.h     Config file parsing and defaults
├── data/
│   ├── words_1k.txt    Top 1 000 English words
│   ├── words_10k.txt   Top 10 000 English words
│   └── quotes.txt      Short passages (one per line)
├── Makefile
└── README.md
```

---

## Building from Source

**Requirements:** `gcc` or `clang`, `make`, a POSIX-compatible system (Linux or macOS).

```bash
git clone https://github.com/yourname/cmonkey.git
cd cmonkey
make
```

The binary is placed at `./cmonkey`. To install system-wide:

```bash
sudo make install   # copies to /usr/local/bin/cmonkey
```

To remove:

```bash
sudo make uninstall
make clean
```

### Compiler options

```bash
make CC=clang              # build with clang instead of gcc
make CFLAGS="-O3 -march=native"   # aggressive optimisation
```

---

## Roadmap

- [ ] Per-run history log (`~/.local/share/cmonkey/history.csv`)
- [ ] ASCII WPM-over-time graph on the results screen
- [ ] Custom word list support (`--list /path/to/file.txt`)
- [ ] Punctuation and number modes
- [ ] Additional languages via UTF-8 word list files
- [ ] Zen mode (no live WPM display during the test)

---

## Contributing

1. Fork the repository and create a feature branch.
2. Keep changes focused — one feature or fix per pull request.
3. Follow the existing code style (C99, 4-space indent, snake_case).
4. All functions in `.h` files must have a doc comment.
5. Run `make clean && make` and verify no warnings before submitting.

Bug reports and feature suggestions are welcome via the issue tracker.

---

## License

MIT — see LICENSE for details.

---

_cmonkey is not affiliated with Monkeytype._
