# Vellum

A local notes and password manager for Windows, built with C++ and Qt.

Vellum keeps your notes and login details in an encrypted file on your computer. A searchable library and a focused editor make room for the content. No account, server, or cloud connection is required.

![Vellum workspace](docs/images/vellum.png)

## Features

- **One vault for notes and passwords.** Create, edit, and search entries from a single workspace.
- **Encrypted files and backups.** Entry contents, titles, and login metadata are encrypted with XChaCha20-Poly1305; Argon2id derives the key from your master passphrase.
- **Local password generation.** Generate passwords or a seven-word master passphrase, with an optional recovery export.
- **Session controls.** Manual and inactivity locking, masked password fields, and a clipboard timeout.
- **Native desktop interface.** A dark, frameless workspace with keyboard shortcuts and no browser runtime.

## Build

Requires Windows x64, Visual Studio C++ tools, CMake 3.25+, Ninja, Qt 6.10.3, and libsodium 1.0.22. Follow the [dependency setup](docs/BUILDING.md#requirements), then run from an **x64 Native Tools Command Prompt**:

```bat
set "QT_ROOT=C:\Qt\6.10.3\msvc2022_64"
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
cmake --install build/windows-release
```

Run `desktop/Vellum.exe`. To produce a distributable ZIP:

```sh
cpack --preset windows-release
```

See [Building Vellum](docs/BUILDING.md) for dependency paths, packaging, and test details.

## Documentation

- [User guide](docs/USAGE.md) - creating a vault, shortcuts, backups, and recovery.
- [Security model](docs/SECURITY.md) - encryption, memory handling, and known limitations.
- [Third-party notices](THIRD-PARTY-NOTICES.txt) - dependency licenses and wordlist attribution.

## Project status

Vellum is in early development and has not received an independent security audit. Use test data while evaluating it. Device sync, browser autofill, and passkeys are not currently supported.

**Keep your master passphrase safe.** There is no password-reset service. Recovery exports contain the passphrase as unencrypted text and should be stored separately from the vault.
