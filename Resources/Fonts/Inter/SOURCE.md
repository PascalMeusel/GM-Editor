# Inter 4.1 runtime font provenance

GM Editor bundles two unmodified static TrueType fonts from the official Inter 4.1 release.

- Upstream repository: <https://github.com/rsms/inter>
- Release: `v4.1`, <https://github.com/rsms/inter/releases/tag/v4.1>
- Release archive: <https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip>
- Archive size: `33707794` bytes
- Archive SHA-256: `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e`
- License: SIL Open Font License 1.1; the complete upstream text is preserved in `LICENSE.txt`.

## Selected unmodified files

| Runtime file | Path inside the official archive | Bytes | SHA-256 |
|---|---|---:|---|
| `Inter-Regular.ttf` | `extras/ttf/Inter-Regular.ttf` | 411640 | `40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82` |
| `Inter-SemiBold.ttf` | `extras/ttf/Inter-SemiBold.ttf` | 419744 | `78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3` |

Downloaded and verified for the 0.9 release candidate on 2026-09-05. No font conversion, subsetting, renaming, or other modification was performed. Source Serif 4 was added separately on 2026-09-19 for recipe-10 atlas lettering; see `../SourceSerif4/SOURCE.md`. Application UI typography remains Inter.
