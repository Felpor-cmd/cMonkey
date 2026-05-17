# Copilot Instructions for `cmonkey`

## Source of truth in this repository

This repository currently contains product docs (`CMONKEY.md` and `cmonkey_project_spec.docx`) but no checked-in `src/`, `Makefile`, or test files yet. Use those two documents as the implementation reference.

## Build, test, and lint commands

- **Current checked-in state:** no runnable build/test/lint targets exist yet.
- **Planned build commands from docs/spec:**
  - `make` (build `cmonkey`)
  - `make clean`
  - `sudo make install`
  - `sudo make uninstall`
- **Single-test command:** no test framework or per-test command is defined in the current repository state.

## High-level architecture (from CMONKEY.md + project spec)

- `main.c` is the orchestration layer: parse CLI flags (`-t`, `-w`, `-q`, `--list`, `--no-color`), initialize config/words/terminal, run the loop, and clean up.
- `engine.c` owns the typing session state machine (`STATE_IDLE -> STATE_RUNNING -> STATE_FINISHED`) and mutable test session state.
- `input.c` handles non-blocking keystroke input and key semantics (printable chars, backspace, escape, enter).
- `terminal.c` is the only terminal I/O/rendering layer (ANSI output + `termios` raw mode + guaranteed restore on exit/signals).
- `metrics.c` contains pure calculations (WPM, accuracy, consistency, elapsed time) and should stay free of I/O.
- `words.c` handles word/quote loading and randomization.
- `config.c` handles `~/.config/cmonkey/config`; CLI flags override config values.

Expected runtime flow: input events update engine state continuously during a run; once finished, metrics are computed from captured keystroke/session data and rendered in a results screen.

## Key conventions specific to this project

- Keep the project **offline-first** and dependency-minimal: libc + POSIX only; no networked features and no ncurses/SDL-style framework unless requirements change.
- Preserve strict module boundaries from the spec:
  - terminal side effects in `terminal.c`
  - state transitions in `engine.c`
  - metric formulas in `metrics.c`
- Match documented style: C99/C11, 4-space indentation, `snake_case`.
- Keep public header APIs documented; docs require exported `.h` functions to include doc comments.
- Keep metric formulas aligned with docs, including the 5-characters-per-word WPM standard.
- Treat terminal restoration as a hard requirement for normal exits and interrupts.
