# Markdown campaign-note exchange

The Data exchange/support area in Settings opens **Exchange campaign notes**.
Export is explicitly for the GM: it contains secrets and private notes. Use the
session Player recap for deliberate player-facing material.

## Export

Prepare an inventory, review the listed notes, select any wanted managed
attachments, and choose a **new directory whose parent already exists**.
The service writes into a unique sibling staging directory and publishes the
completed directory. Existing destinations are refused and preserved. An error
leaves the destination unpublished; an interrupted process can leave a uniquely
named staging directory for manual inspection/removal.

`INDEX.md` maps stable entity IDs and types to note files. Filenames encode stable
IDs, so renaming a title does not rename its exported file. Covered prose is the
campaign description; story threads and scenes; preparation, live and wrap-up
session notes; structured outcomes and carried follow-ups; NPCs; places;
objectives; clues; handouts; timeline events; factions; deities; and nations.
Note bodies preserve their text, surrounded by labelled Markdown sections.
This is not a database backup or a complete serialization of mechanical records,
maps, inventories, arbitrary custom properties, undo receipts or entity links.
Use campaign package export for complete supported campaign interchange.

Attachments default to **none selected**. The inventory reads only the selected
campaign's managed `Assets` buckets. It does not copy arbitrary paths from
handouts, Markdown links or the campaign filesystem. Recovery files, backups,
databases and unrelated campaign data are outside the inventory. Supported
attachment extensions are common images (including SVG), audio, PDF, `.txt` and
`.md`; other managed files are listed as omitted. Files are copied unchanged,
not interpreted, sanitized or converted. Numeric output names are mapped back
to the original managed relative names in the index.

Symlinks, junctions, reparse points and destination overlap with campaign storage
are rejected. Reviewed note content and selected asset sizes/content fingerprints
are checked again before export; copied asset bytes are checked before publishing.
The existing FNV content fingerprint detects ordinary changes and is not a
cryptographic authenticity guarantee. Preview limits are 20,000 note records,
an estimated 64 MiB formatted-text budget, 10,000 managed entries, 64 MiB per attachment and
256 MiB of inventoried attachment bytes. Oversized/unsupported assets are omitted
with warnings, not silently included. Disk inventory, reading, hashing and copying
run in a cancellable background task. Progress shows processed files and bytes.
Cancel or Return revokes the pending action immediately; owned temporary files
are cleaned in the background. A terminated process may leave staging behind.
Capturing campaign text and the final transactional save still run on the main
thread; this does not establish a maximum input latency for large campaigns.

## Preview-first import

Choose one `.md` file, inspect its literal text preview, acknowledge the preview,
then import it as a **private story-thread note**. UTF-8 text up to 4 MiB is accepted,
including Unicode, an existing UTF-8 BOM and original line endings. Invalid UTF-8
and NUL bytes are refused instead of being silently transcoded. A changed source
requires a fresh preview. The import never runs HTML, resolves links, reads linked
local files or downloads remote content.

The source filename supplies the new title. Exact source bytes are stored in
`StoryThreadModel::gmNotes`; persisted custom properties retain the original text,
source filename, format and content fingerprint. The absolute source path is not
persisted. Saving uses a staged campaign copy and the normal transactional
persistence callback. Failure leaves the current campaign unchanged. Retrying the
same reviewed request returns the existing note instead of creating another one.
A new preview is a new deliberate import; matching filenames are not merged.

The repository's `StoryDocumentModel` is currently an unused struct without
campaign-object storage or database persistence. This feature deliberately uses
the existing persisted story-thread model and requires no schema migration.
Imported YAML frontmatter, wiki links, embedded Markdown and external IDs remain
ordinary text. There is no entity reconstruction, bidirectional sync, folder
import or automatic attachment import. Edit imported notes in the Story library.

## Update previously exported notes

Select **Include editable story-thread notes** when exporting to add a bounded
editing workflow alongside the reading copies. Only existing story-thread GM
note text can be updated. Titles, scene text, links, rules, maps and attachments
remain outside this operation. If a note exceeds the editable limits, the preview
explains why; clear the editable option to export ordinary reading copies.

1. Edit the relevant file in `editable/` using a text editor. `INDEX.md` identifies
   the stable note IDs. Do not edit `baseline/` or `EDITABLE_NOTES.v1`, and do not
   use a page in `notes/` as an editable companion.
2. Return to the original campaign and open **Update exported notes**. Choose the
   export folder and select **Preview changes**.
3. Compare the original text, current campaign text and edited file. Select the
   compatible notes you want to change, review that selection, then apply it.

Matching uses campaign and note IDs, never titles. Unchanged files require no
action. Already applied text does not cause another save. A note changed both
inside and outside the application is a conflict and cannot be selected; copy
the desired wording into the campaign deliberately and export a new baseline.
Missing, archived or duplicate identities are unavailable, not recreated.

Editable files preserve exact UTF-8 bytes, including BOM, line endings and empty
text. Limits are 10,000 notes, 4 MiB per note, 64 MiB combined original/edited
text, 4 KiB titles and an 8 MiB versioned manifest. Changing a reviewed file
requires a new preview. A failed refresh keeps the earlier preview read-only.
Changing paths, selections or campaign context revokes the earlier approval.

Selected changes use one transactional save. Failed saves leave current note
text unchanged and permit an explicit retry. A context change during a completed
save is reported as saved with newer live state retained, requiring a fresh
review. Private note metadata retains the update format, original/edited
fingerprints and source directory as provenance. This is a deliberate bounded
round trip, not continuous synchronization with another application.

## Verification

`campaign_markdown_exchange_tests` / CTest `campaign_markdown_exchange` cover
indexed export scope, stable paths, selected attachments, unsafe/stale inputs,
destination preservation, exact-text import, preview isolation, real SQLite
rollback, reload/provenance and retry deduplication. Test execution results belong
in the implementation roadmap and the dated improvement record. Additional
cases exercise editable identity/conflict classification, exact-byte manifests,
multi-note rollback, callback changes, cancellation, stale completion and
directory collisions. CPU UI cases exercise actual compact EN/DE controls and
keyboard actions; they do not establish native display or hardware performance.
The test optionally exercises symbolic links
when the host grants permission; unsupported link creation is not a claim that
the Windows junction case was observed. No desktop or human usability test is
implied by these automated cases.
