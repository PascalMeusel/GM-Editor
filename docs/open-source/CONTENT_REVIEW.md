# Content included in the community preview

The owner selected GPL-3.0-or-later for code and CC BY 4.0 for rights held in
project artwork on 30 September 2026. The owner knew of no externally supplied
artwork and asked for the generation records to be checked. They authorized
omitting or replacing unresolved content and preparing both source and Windows
downloads. This is a recorded release selection, not a guarantee against every
possible third-party claim.

## Artwork provenance

- **FantasyNeutral V3:** 424 runtime images from the project ImageGen production
  sheets. Each current image matches its immutable snapshot SHA-256. The recorded
  prompts use the project's temperate-grass and road/river sheets as references,
  request original generic fantasy terrain and exclude text and signatures.
- **V3.1:** 60 deterministic composites. All declared input asset IDs resolve to
  the V3 collection. They inherit its rights basis; current file hashes are now
  fixed in the release ledger.
- **V3.2 and V3.3:** seven generated world-map overlay images, matching their
  recorded hashes. Prompt records describe original mountain ranges and a neutral
  watercolor surface, generated with OpenAI ImageGen.
- **V2 road compatibility:** the five packed road atlas pages and their manifest
  retain 92 accepted project road cells. All IDs resolve to the project's road
  source ledger. These are compatibility assets from the same project generation
  pipeline, not separately downloaded asset packs.
- **Portraits:** 22 sheets, eight portraits each, covering eleven species in two
  collections. Company sheets have generation IDs, prompts and matching hashes.
  Distinct sheets match the runtime hashes; their only recorded image reference
  is the collection's own generated human sheet. Included species: Human, Elf,
  Dwarf, Orc, Tiefling, Dragonborn, Gnome, Goliath, Halfling, Half-Elf and Half-Orc.
- **Icon:** the original, code-authored session-book SVG and raster exports. The
  old icon's origin was not established, so it is omitted. No external font,
  photograph or stock image is embedded in the replacement SVG.

The generation records contain no identified external image source for the
selected artwork. This finding combines those records with the owner's account;
it does not establish that every AI output is unique or copyrightable. OpenAI's
[Europe terms](https://openai.com/policies/eu-terms-of-use/) allocate output rights
between the user and OpenAI to the extent permitted by law, while leaving input
permissions and third-party rights with the user. That supports the project's
grant of rights it holds, not a claim of exclusive ownership over all pixels.

See [artwork terms](../../ARTWORK_LICENSE.md). The current hash-bound decisions
supersede old pending-rights flags only for the selected files. Source-generation
experiments, rejected candidates, nonselected portrait artwork, the old icon and
private marketing material are excluded. Original private files are preserved.

## Rules and catalog

The nine offered default species use SRD 5.2.1. Aasimar was removed from that list.
Existing saved names and numeric portrait IDs remain compatible; omitted portrait
artwork does not erase campaign data. The picker offers installed artwork only.

The 172 built-in equipment, monster and spell names were cross-checked against
the official SRD 5.1 and 5.2.1 PDFs. All have a corresponding name in at least one
document. The nine abbreviated monster entries use older SRD statistics; some
spell values and species use the newer document. Both attributions are now
provided in the application and [notices](../../THIRD_PARTY_NOTICES.md).

The checked rules modules contain selected formulas and abbreviated rule summaries,
not a complete licensed rulebook. Their mixture of editions and simplifications
must not be advertised as exact 2024 rules support. Name matching establishes
catalog scope, not correctness of every implementation. No non-SRD default rule
content was identified beyond the removed species entry; legacy portrait names
are retained as compatibility identifiers, without their nonselected artwork or
species rules.

On 3 October the compiled rules display name changed to “D&D assistance
(mixed SRD 5.1 / 5.2.1)”. Its former display name remains a loading alias;
stable ruleset IDs and version pins stay unchanged. The hash-bound decision
for `DND5eIdentityDefaults.hpp` was refreshed after checking that the delta
changes only those two name/alias lines; formulas, species and attribution
text are unchanged. No new third-party rule content was introduced by this delta.

Official sources: [SRD downloads and FAQ](https://www.dndbeyond.com/srd),
[SRD 5.1](https://media.dndbeyond.com/compendium-images/srd/5.1/SRD_CC_v5.1.pdf),
[SRD 5.2.1](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).

## Audio and fonts

The release catalog selects 69 CC0 audio files from 21 OpenGameArt source pages.
The earlier source audit matched their bytes to those sources; the source pages
are checked again for this release selection. Ancient Power of Serpents and Lost
in the Desert are omitted because their documented questions remain unresolved.
This does not claim that CC0 eliminates every possible rights dispute.

The unchanged Inter and Source Serif fonts retain their OFL 1.1 licenses and
recorded upstream hashes. They are not relicensed as project artwork.

## Reproducible selection

`distribution/community-preview-resources.json` selects resources explicitly.
`distribution/open-source-decisions.json` binds each approval to exact bytes,
license and evidence. `tools/prepare_open_source.py --public` still refuses an
unresolved selected file. Changing artwork requires a new supported decision.
Neither tool creates a public repository or publishes a release.
