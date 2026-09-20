<h1 align="center">Vellum</h1>

<p align="center">
  A private workspace for notes and passwords.<br>
  Built for Windows. Stored on your computer.
</p>

<p align="center">
  <a href="#getting-started">Getting started</a> &nbsp;&middot;&nbsp;
  <a href="docs/USAGE.md">User guide</a> &nbsp;&middot;&nbsp;
  <a href="docs/BUILDING.md">Building</a> &nbsp;&middot;&nbsp;
  <a href="docs/SECURITY.md">Security</a>
</p>

![Vellum's dark workspace showing fictional notes and login entries](docs/images/vellum.png)

Vellum brings notes and login details into one encrypted vault. Search your library, write a note, or copy a password without leaving the desktop app. Your master passphrase unlocks the file; no account or server is required.

> [!IMPORTANT]
> Vellum is in early development and has not received an independent security audit. Use test data while evaluating it.

## Inside Vellum

- **Notes and logins together.** A searchable library with separate views for passwords and notes.
- **Encrypted storage.** Vault contents and entry metadata are protected with XChaCha20-Poly1305 and an Argon2id-derived key.
- **Local generation.** Create random passwords or a seven-word master passphrase on your computer.
- **Backups and recovery.** Save encrypted backups, with an optional master-passphrase export for safekeeping.
- **Desktop controls.** Manual and inactivity locking, masked passwords, clipboard timeout, and keyboard shortcuts in a dark, frameless interface.

## Getting started

### Build and run

Vellum currently targets **Windows x64**. Before building, install Visual Studio C++ tools, CMake 3.25+, Ninja, Qt 6.10.3, and libsodium 1.0.22. Follow the [dependency setup](docs/BUILDING.md#requirements) for the required paths.

From an **x64 Native Tools Command Prompt for Visual Studio**, run:

```bat
set "QT_ROOT=C:\Qt\6.10.3\msvc2022_64"
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
cmake --install build/windows-release
```

Open `desktop/Vellum.exe`, choose **Create new**, and follow the passphrase and recovery steps. See the [user guide](docs/USAGE.md) for working with entries and backups.

To package a distributable ZIP, run `cpack --preset windows-release`. Full build, test, and packaging instructions are in [Building Vellum](docs/BUILDING.md).

### Everyday shortcuts

| Action | Shortcut |
| --- | --- |
| Search the vault | `Ctrl+K` |
| Save an entry | `Ctrl+S` |
| Lock the vault | `Ctrl+L` |

## Keep your recovery copy separate

An encrypted backup is a copy of the vault. A recovery export is **unencrypted text containing the master passphrase**. Store it separately from the vault and backups. There is no password-reset service.

Read the [security model](docs/SECURITY.md) for the encryption design, memory and clipboard limitations, and verification still needed. Device sync, browser autofill, and passkeys are not currently supported.

## Acknowledgements

Built with C++20, Qt Widgets, and libsodium. Passphrase generation uses the EFF Long Wordlist. Dependency licenses and attribution are collected in [third-party notices](THIRD-PARTY-NOTICES.txt).
