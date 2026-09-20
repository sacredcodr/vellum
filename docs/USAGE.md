# Using Vellum

## Create or open a vault

Extract the complete application package and run `Vellum.exe`. Keep its DLLs and `platforms` directory alongside it. If you built from source, run `desktop/Vellum.exe`.

1. Choose **Create new** and select a location for the `.vault` file.
2. Enter and confirm a master passphrase, or choose **Generate passphrase**.
3. Continue to recovery. Export a recovery copy or confirm that you have recorded the passphrase somewhere safe.
4. Choose **Create vault**.

Use **Open vault** to open an existing vault or encrypted backup with its master passphrase. There is no password-reset service.

## Work with entries

Choose **New > Password** or **New > Secure note**. Save edits with **Save changes** or `Ctrl+S`. Search covers titles and usernames; the sidebar filters distinguish passwords and notes.

Password fields are masked by default. **Show** reveals them, **Generate** creates a random password, and **Copy password** uses a clipboard timeout. Other software can still read the clipboard before it is cleared.

| Shortcut | Action |
| --- | --- |
| `Ctrl+K` | Search the vault |
| `Ctrl+S` | Save the current entry |
| `Ctrl+L` | Lock the vault |

## Back up and recover

Choose **Library > Create encrypted backup** to write a separate encrypted vault file. Test the backup by opening it with your master passphrase. Store a copy on a separate trusted device; a second file on the same disk does not protect against disk loss.

A **recovery copy** is an unencrypted text file containing the actual master passphrase. It is not an encrypted backup or a separate reset code. Anyone with the recovery copy and the vault can unlock its contents. Keep the recovery copy separate from the vault and backups.

Losing your passphrase and all recovery copies means losing access. Deleting an entry does not remove it from earlier backups or filesystem snapshots.

See the [security model](SECURITY.md) for implementation details and limitations.
