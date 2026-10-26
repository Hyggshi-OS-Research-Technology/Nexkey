# Contributing to NexKey

Thank you for your interest in contributing to **NexKey**! Whether you are reporting a bug, proposing a new feature, writing documentation, or submitting code, your contributions help make Vietnamese input on Linux faster, smoother, and more reliable.

Please take a moment to read this guide before contributing.

---

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How Can I Contribute?](#how-can-i-contribute)
  - [Reporting Bugs](#reporting-bugs)
  - [Suggesting Enhancements](#suggesting-enhancements)
  - [Documentation Improvements](#documentation-improvements)
  - [Code Contributions](#code-contributions)
- [Development Setup](#development-setup)
  - [Prerequisites](#prerequisites)
  - [Building from Source](#building-from-source)
  - [Running the Test Suite](#running-the-test-suite)
- [Architecture & Code Standards](#architecture--code-standards)
  - [Project Architecture](#project-architecture)
  - [C++23 Guidelines](#c23-guidelines)
  - [Code Formatting](#code-formatting)
  - [Compiler Warnings](#compiler-warnings)
- [Pull Request Process](#pull-request-process)
  - [Branch Naming](#branch-naming)
  - [Commit Messages](#commit-messages)
  - [PR Checklist](#pr-checklist)
- [License](#license)

---

## Code of Conduct

This project and everyone participating in it are governed by our [Code of Conduct](CODE_OF_CONDUCT.md) (Bản tiếng Việt: [CODE_OF_CONDUCT.vi.md](CODE_OF_CONDUCT.vi.md)). By participating, you are expected to uphold this code. Please report unacceptable behavior via project issues or directly to the maintainers.

---

## How Can I Contribute?

### Reporting Bugs

Before submitting a bug report, please check existing [GitHub Issues](https://github.com/Hyggshi-OS-Research-Technology/Nexkey/issues) to see if the issue has already been reported.

When creating an issue, please include:
1. **Clear description**: What happened vs. what you expected to happen.
2. **Steps to reproduce**: Exact keystrokes, typing method (Telex, VNI, VIQR), output charset, and application where the issue occurred.
3. **System environment**:
   - Linux distribution and version (e.g., Ubuntu 24.04, Arch Linux, Fedora 40).
   - Display server: Wayland or X11.
   - Input framework: Fcitx5 or IBus (and version).
   - Application affected: (e.g., Chrome, Firefox, VS Code, GNOME Text Editor, Terminal).
4. **Log output**: Relevant output from terminal commands or journal logs (`fcitx5 -d --replace` or `journalctl --user -u fcitx5`).

### Suggesting Enhancements

We welcome suggestions for new features, keyboard layouts, or performance improvements!

When submitting a feature request:
- Explain **why** the feature is useful and what problem it solves.
- Describe the expected behavior or user interface.
- Consider NexKey's core philosophy: **ultra-fast native direct typing**, **zero-dependency core**, and **clean architectural separation**.

### Documentation Improvements

Documentation is as important as code. You can contribute by:
- Fixing typos, grammatical issues, or clarifying technical explanations.
- Updating or expanding usage guides (e.g., [CONFIGURATION.md](docs/CONFIGURATION.md), [ARCHITECTURE.md](docs/ARCHITECTURE.md)).
- Improving translations between English and Vietnamese.

### Code Contributions

If you would like to contribute code:
1. Fork the repository and create your branch from `main`.
2. Follow our architecture boundaries and coding standards.
3. Ensure all existing and new unit tests pass.
4. Submit a Pull Request.

---

## Development Setup

### Prerequisites

| Component | Minimum Requirement | Notes |
| --- | --- | --- |
| **Compiler** | GCC 12+ or Clang 15+ | Must support C++23 |
| **Build System** | CMake 3.21+ | Ninja build tool recommended |
| **GUI (Optional)** | Qt 6.2+ | `QtCore`, `QtGui`, `QtWidgets`, `QtDBus` |
| **Fcitx5 Addon (Optional)** | Fcitx5 5.0+ dev packages | `Fcitx5Core`, `Fcitx5Config`, `Fcitx5Utils` |
| **IBus Engine (Optional)** | IBus 1.5+ dev packages | `ibus-1.0` |

On Debian / Ubuntu:
```bash
./scripts/install-build-deps.sh
```

Or manually:
```bash
sudo apt install build-essential cmake ninja-build \
    qt6-base-dev libqt6dbus6 \
    extra-cmake-modules fcitx5-modules-dev libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev \
    libibus-1.0-dev
```

### Building from Source

```bash
# Clone the repository
git clone https://github.com/Hyggshi-OS-Research-Technology/Nexkey.git
cd Nexkey

# Configure with CMake (warnings as errors enabled for development)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DNEXKEY_WARNINGS_AS_ERRORS=ON

# Compile in parallel
cmake --build build --parallel
```

### Running the Test Suite

NexKey comes with a comprehensive built-in unit test suite covering syllable parsing, tone placement, method parsing (Telex, VNI, VIQR), Unicode encoding, and settings serialization:

```bash
ctest --test-dir build --output-on-failure
```

Or run the test binary directly:
```bash
./build/tests/nexkey-tests
```

---

## Architecture & Code Standards

### Project Architecture

NexKey is organized into strictly bounded layers:

```
Nexkey/
├── core/                 # C++23 input engine, charsets, settings (ZERO external dependencies)
├── cli/                  # Command-line utility (`nexkey`) for scripting and conversion
├── gui/                  # Qt6 system tray, settings dialog, and smart emoji picker
├── integrations/
│   ├── fcitx5/           # Fcitx5 input method addon (module)
│   └── ibus/             # IBus input method engine
├── tests/                # Unit test harness and comprehensive test cases
├── packaging/            # Debian packaging, AppImage assets, desktop integration
├── scripts/              # Helper shell scripts for build, test, and packaging
└── docs/                 # In-depth architectural, configuration, and module documentation
```

**Key Architectural Rules:**
- **`core/` must NEVER depend on Qt, Fcitx5, IBus, X11, or Wayland.** It must remain a pure, dependency-free C++23 library.
- Business logic for Vietnamese syllable analysis, toneless rendering, and spelling rules belongs in `core/`.
- Frontend integrations (`gui/`, `integrations/fcitx5/`, `integrations/ibus/`) should only translate input context events and invoke the core `Engine`.

### C++23 Guidelines

- Use modern C++ features: `std::string_view`, `std::span`, `constexpr`, structured bindings, and `<ranges>`.
- Mark non-mutating member functions as `const`.
- Mark pure/result-yielding functions with `[[nodiscard]]`.
- Always check return values of `[[nodiscard]]` functions or handle error states explicitly.
- Avoid raw pointers for resource ownership; use standard smart pointers (`std::unique_ptr`, `std::shared_ptr`) or RAII wrappers.

### Code Formatting

NexKey uses `.clang-format` based on the LLVM style with 4-space indentation.

Before committing, format your code using:
```bash
clang-format -i core/src/* core/include/nexkey/* cli/* gui/* integrations/fcitx5/* integrations/ibus/* tests/*
```

Or check changes with:
```bash
git diff -U0 --no-color 'HEAD^' | clang-format-diff -p1
```

### Compiler Warnings

NexKey compiles with aggressive compiler warnings:
`-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-qual -Wconversion -Wsign-conversion -Wunused -Werror`.

Ensure your code compiles cleanly without any warnings when `-DNEXKEY_WARNINGS_AS_ERRORS=ON` is enabled.

---

## Pull Request Process

1. **Keep PRs focused**: Each PR should address a single bug fix, refactor, or feature.
2. **Add unit tests**: If you fix a syllable/tone bug or add a new input feature, add a corresponding test in `tests/`.
3. **Verify locally**:
   ```bash
   cmake --build build --parallel && ctest --test-dir build --output-on-failure
   ```
4. **Follow commit conventions**:
   - `feat: add Microsoft layout keyboard preset`
   - `fix: prevent character duplication in browser omnibox`
   - `docs: update configuration guide for custom keymaps`
   - `test: add syllable tests for open diphthong tone placement`
   - `refactor: optimize UTF-8 delta prefix comparison`
5. **Open a Pull Request**: Provide a clear description of what changed, why the change is necessary, and any relevant issue numbers (e.g., `Closes #12`).

---

## License

By contributing to NexKey, you agree that your contributions will be licensed under the project's [MIT License](LICENSE).
