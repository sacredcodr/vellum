# Using Vellum

## Create or open a vault

Extract the complete application package and run `Vellum.exe`. Keep its DLLs and `platforms` directory alongside it. If you built from source, run `desktop/Vellum.exe`.

1. Choose **Create new** and select a location for the `.vault` file.
2. Enter and confirm a master passphrase, or choose **Generate passphrase**.
3. Continue to recovery. Export a recovery copy or confirm that you have recorded the passphrase somewhere safe.
4. Choose **Create vault**.

Use **Open vault** to open an existing vault or encrypted backup with its master passphrase. There is no password-reset service.

## Work with entries

Choose **New > Password** or **New > Secure note**. Save edits with **Save changes** or `Ctrl+S`. Search covers saved titles, usernames, websites, and note text; the sidebar filters distinguish passwords and notes. Every search word must match, in any order and ignoring case. Password values are excluded. Unsaved edits remain in the editor and become searchable when saved.

The sidebar shows matches out of the total for the selected filter. If nothing matches, choose **Clear search** or try fewer words. Searching keeps the current editor open; switching to another result prompts you to save, discard, or cancel if needed.

Password fields are masked by default. **Show** reveals them, **Generate** creates a random password, and **Copy password** uses a clipboard timeout. Other software can still read the clipboard before it is cleared.

## Write and format notes

Use the toolbar above the note body to apply **bold**, *italic*, underline, headings, bulleted or numbered lists, and text colors. Select text to format it, or choose a style before typing. Paragraph styles and list controls apply to the current paragraph or selected paragraphs. Click an active list button again to return to normal paragraphs. The eraser button (**Clear inline formatting**) removes inline formatting from the selection; choose **Normal text** to reset a heading. Hover over a control for its name and shortcut. The color button shows the current text color, and its menu includes named swatches.

Formatting is saved with **Save changes** or `Ctrl+S` inside the encrypted vault. Existing plain-text notes stay readable. Search uses saved note text and ignores formatting. Text pasted from other apps is inserted as plain text; images and external resources are not loaded.

Undo and redo apply within the current entry. Their history is cleared when switching entries or locking. Older Vellum builds can read the plain-text copy but cannot edit formatting; if an older build changes the text, this build uses that newer text and drops stale formatting when saving.

| Shortcut | Action |
| --- | --- |
| `Ctrl+K` | Search the vault |
| `Down` in search | Select the first result and focus the list |
| `Enter` in search or results | Open the result and focus its title |
| `Escape` in search | Clear the query |
| `Ctrl+S` | Save the current entry |
| `Ctrl+L` | Lock the vault |
| `Ctrl+B` / `Ctrl+I` / `Ctrl+U` in notes | Bold / italic / underline |
| `Ctrl+Z` / `Ctrl+Y` in notes | Undo / redo |

## Back up and recover

Choose **Library > Create encrypted backup** to write a separate encrypted vault file. Test the backup by opening it with your master passphrase. Store a copy on a separate trusted device; a second file on the same disk does not protect against disk loss.

A **recovery copy** is an unencrypted text file containing the actual master passphrase. It is not an encrypted backup or a separate reset code. Anyone with the recovery copy and the vault can unlock its contents. Keep the recovery copy separate from the vault and backups.

Losing your passphrase and all recovery copies means losing access. Deleting an entry does not remove it from earlier backups or filesystem snapshots.

See the [security model](SECURITY.md) for implementation details and limitations.
