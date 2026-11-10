# Third party notices

These notices describe licenses that must accompany the selected release files.
They do not clear the private artwork collection for redistribution. The release
audit determines which assets may be included.

## Code dependencies

| Dependency | Pinned source | License location |
|---|---|---|
| Dear ImGui | `a28cb615ff2c2ed8901397b00762d557cae24760` | `extern/imgui/LICENSE.txt` (MIT) |
| GLFW | `63a7e8b7f82497b0459acba5c1ce7f39aa2bc0e8` | `extern/glfw/LICENSE.md` (zlib/libpng) |
| SQLiteCpp | `4cb0bf68b75b2f8b749ec5dc77f152f8aa62ec0e` | `LICENSE.txt` in the fetched SQLiteCpp source (MIT) |
| SQLite | Bundled by that SQLiteCpp revision | [Upstream copyright statement](https://www.sqlite.org/copyright.html) |
| stb_image and stb_image_write | Headers in the pinned dependencies | License text embedded in those headers; preserve it. |

Local dependency changes are recorded in `cmake/patches/`. They are applied in the
build directory; upstream submodules and their notices remain intact. A packaged
binary must include the full dependency license texts, not just this table.

## Fonts

Unmodified Inter 4.1 Regular and SemiBold are under SIL OFL 1.1. The full license
and pinned source hashes are in `Resources/Fonts/Inter/`.

Unmodified Source Serif 4.005 Regular is under SIL OFL 1.1. The full license and
pinned source hashes are in `Resources/Fonts/SourceSerif4/`. The reserved font
name is Source; the font has not been renamed, subsetted or modified.

## System Reference Document

This work includes material taken from the System Reference Document 5.1
(“SRD 5.1”) by Wizards of the Coast LLC and available at
https://dnd.wizards.com/resources/systems-reference-document. The SRD 5.1 is
licensed under the Creative Commons Attribution 4.0 International License
available at https://creativecommons.org/licenses/by/4.0/legalcode.

This work includes material from the System Reference Document 5.2.1
(“SRD 5.2.1”) by Wizards of the Coast LLC, available at
https://www.dndbeyond.com/srd. The SRD 5.2.1 is licensed under the Creative Commons
Attribution 4.0 International License, available at
https://creativecommons.org/licenses/by/4.0/legalcode.

Modifications to both versions: selected rules, statistics and descriptions have been adapted into
data structures, calculations and interface text. This software is an incomplete
implementation; do not treat its behavior as the complete SRD. Material outside
the SRD requires a separate basis for redistribution. An attribution statement
does not itself establish that every bundled entry is within the SRD.

## Audio and artwork

Audio source records are in `Resources/Audio/ATTRIBUTION.md` and `catalog.psv`.
The community preview selects 69 CC0 files and omits two entries with unresolved
source questions. Private copies of those entries are not part of the release.

Selected project map/portrait artwork and the original session-book icon are
licensed under CC BY 4.0, to the extent the project holds rights. The scope,
AI-generated origin and attribution are in [ARTWORK_LICENSE.md](ARTWORK_LICENSE.md).
The hash-bound release decisions are in `distribution/open-source-decisions.json`.
Earlier embedded rights-review flags are historical; the current decision ledger
supersedes those flags only for its exact listed file bytes. It does not change
map snapshot IDs, visual acceptance or omitted assets.
