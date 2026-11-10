# GM Editor community preview

Free Windows desktop software for preparing tabletop RPG sessions, creating
connected maps and presenting selected information to players at the table.
This is an early community preview, with experimental features and known limits.

## Start the Windows application

1. Extract the entire Windows ZIP to a writable folder. Keep `Resources` beside
   `TTRPG_Editor.exe`; running the executable from inside the ZIP is unsupported.
2. Open `TTRPG_Editor.exe`. The package targets Windows x64 and requires an
   OpenGL-capable graphics driver. It needs neither a developer installation nor
   an AI account. The Microsoft C++ runtime is linked into this preview.
3. Create a test campaign or use the onboarding sample before importing your own
   campaign. Keep an independent backup of existing data.
4. Prepare scenes, people, places and maps, then use the Live workspace to run
   your session. Open the separate Player Display deliberately when ready and
   review what you share.

Campaigns and settings are stored per user, normally under
`%LOCALAPPDATA%\GM Editor`; they are separate from the extracted program folder.
Use the application's export/backup tools before replacing an existing version.

The app includes 176 portraits (two collections of eleven species) and 69 starter
audio files. Existing saved portrait IDs remain intact even when a particular
portrait is outside this preview's collection. Only installed species appear in
the portrait selector. Custom campaign content remains yours.

This build is unsigned. Windows may show a publisher/reputation warning; code
signing and testing on a second physical machine are not claimed. Obtain the
release only from its eventual official project page and compare the supplied
SHA-256 checksum. No donation or payment is required.

## Hinweise auf Deutsch

Das Programm ist eine kostenlose Vorschau für Windows x64. Das gesamte ZIP
entpacken und `TTRPG_Editor.exe` im entpackten Ordner starten. Der Ordner
`Resources` muss daneben bleiben. Zunächst eine Testkampagne verwenden und
bestehende Daten unabhängig sichern.

Die Spieleranzeige ist ein lokales zweites Fenster, das bewusst geöffnet wird.
Es gibt noch keinen Online-Mehrspielermodus. Englische und deutsche Oberfläche
sind vorhanden; einzelne Fachansichten sind noch nicht vollständig übersetzt.

## Source, permissions and limitations

The paired source ZIP includes the exact upstream code dependencies needed for
the application; see its build guide. Project code is GPL-3.0-or-later. Selected
project artwork is CC BY 4.0, fonts are OFL 1.1 and the starter audio is CC0.
Complete scope and required credits accompany both archives.

See [known limitations](KNOWN_LIMITATIONS.md) and [content selection](CONTENT_REVIEW.md).
The preparation is local until the owner reviews and publishes the candidate.
