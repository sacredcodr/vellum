# Security model

Vellum is an unaudited local Windows vault. It is intended to protect saved vault files against casual inspection and to detect changes to authenticated data. It cannot protect an unlocked computer from malware or an administrator.

## File encryption

- libsodium 1.0.22 provides the cryptographic primitives.
- Argon2id v1.3 derives a 256-bit key with 3 passes and 256 MiB of memory.
- New user-supplied passphrases must contain at least six unrelated words or 24 mixed characters and are checked for common, repeated, and sequential patterns. Existing vaults remain unlockable regardless of their original passphrase strength.
- XChaCha20-Poly1305 authenticates a versioned 64-byte header and encrypts all entry content, including titles and URLs.
- Each save uses a fresh random nonce.
- Unknown derivation parameters and oversized files are rejected before expensive processing. The maximum vault file size is 16 MiB.
- A process lock prevents cooperative concurrent writers. A ciphertext fingerprint detects external changes before a save.
- QSaveFile atomic replacement has direct-write fallback disabled.

## Secrets during use

Keys use libsodium protected allocation and are wiped on release. Failure to lock key memory refuses unlock. Temporary serialized and decrypted byte buffers are explicitly wiped.

Qt widgets, strings, and JSON objects can retain plaintext copies in memory. Complete memory erasure, protection against swap/hibernation/crash dumps, and resistance to malware are not guaranteed.

The app requests clipboard history/cloud exclusion and clears copied secrets after 20 seconds only if clipboard ownership is unchanged. Third-party clipboard managers can still capture contents. Crashes or forced termination can prevent clearing.

Setup fields clear on lock and inactivity. During vault creation, recoverable validation or save errors preserve the entered passphrase so the user can correct the issue and retry. Successful creation clears the setup fields.

## Recovery and backups

An encrypted backup is another copy of the encrypted vault. A recovery export is a deliberately unencrypted copy of the master passphrase, created only through an explicit warning and file selection. Recovery copies require an NTFS or ReFS destination, receive a protected DACL for the current Windows account, use write-through I/O, and are flushed before success is reported. Neither export overwrites an existing file.

There is no reset service, protection against replaying an older valid vault, or guarantee that deleting an entry removes it from old backups, filesystem snapshots, or storage media.

## Verification status

Automated tests exercise authentication, tampering, recovery, input validation, locking callbacks, edit preservation, and dialog cancellation. Offscreen renders are used to inspect layout.

Pending checks include real Windows session/suspend transitions, clipboard/history behavior, idle timing, compositor dragging/resizing, high-DPI and accessibility behavior, disk-exhaustion/power-loss fault injection, and independent security review.

Configure-time verification pins the exact Qt and libsodium runtime, import-library, plugin, and header files used by the audited dependency set. A mismatch stops the build. Development packages are visibly named `UNSIGNED-DEVELOPMENT`; an official package can only be configured with a certificate thumbprint and signs and verifies `Vellum.exe` through Windows SignTool.
