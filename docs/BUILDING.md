# Building Vellum

### Requirements

- Windows x64
- Visual Studio with the **Desktop development with C++** workload
- CMake 3.25 or newer and Ninja, available on `PATH`
- Qt 6.10.3 for MSVC x64, including Core, Widgets, and Test
- [libsodium 1.0.22 MSVC binaries](https://github.com/jedisct1/libsodium/releases/tag/1.0.22-RELEASE)

Place the libsodium package so these paths exist:

```text
.deps/sodium/libsodium/include/sodium.h
.deps/sodium/libsodium/x64/Release/v143/dynamic/libsodium.lib
.deps/sodium/libsodium/x64/Release/v143/dynamic/libsodium.dll
```

Open an **x64 Native Tools Command Prompt for Visual Studio**. Set `QT_ROOT` to your Qt MSVC installation, then configure and build:

```bat
set "QT_ROOT=C:\Qt\6.10.3\msvc2022_64"
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

Install the runnable app and its runtime dependencies to `desktop/`:

```sh
cmake --install build/windows-release
```

Create a distributable ZIP with CPack:

```sh
cpack --preset windows-release
```

The archive is written to `build/windows-release/`. Normal builds are named `UNSIGNED-DEVELOPMENT` so they cannot be mistaken for an official release. It includes the runtime libraries, platform plugins, documentation, and dependency notices. Build configuration and packaging are defined entirely in CMake.

The configure step verifies SHA-256 hashes for the exact Qt 6.10.3 and libsodium 1.0.22 files shipped by Vellum. A missing or altered dependency stops the build. When intentionally updating either dependency, verify the upstream release first and update [the dependency manifest](../cmake/DependencyHashes.cmake) in the same review.

### Signed official release

Official packages require a trusted Authenticode certificate in the Windows certificate store and Windows SDK `signtool` on `PATH`. Configure with the certificate's 40-character SHA-1 thumbprint:

```bat
cmake --preset windows-release -DVELLUM_OFFICIAL_RELEASE=ON -DVELLUM_SIGN_CERT_SHA1=0123456789ABCDEF0123456789ABCDEF01234567
cmake --build --preset windows-release
ctest --preset windows-release
cpack --preset windows-release
```

The build signs `Vellum.exe` with SHA-256, applies an RFC 3161 timestamp, and runs Authenticode verification before packaging. Configuration fails when the certificate thumbprint or SignTool is unavailable.

### Tests

The test preset runs both storage and interface regression suites. They cover encrypted round trips, tampering, incorrect passphrases, backup recovery, concurrent writers, entry editing, setup validation, recovery export, failed-save retries, and simulated session locking.

To rerun the compiled tests with the same `QT_ROOT` setting:

```powershell
ctest --preset windows-release
```

An isolated preview mode renders fictional data into a new output directory:

```powershell
./desktop/Vellum.exe --self-test --data-dir ./test-artifacts/preview
```

Use a fresh directory each time. These are offscreen renders, not verification of live Windows compositor behavior.
