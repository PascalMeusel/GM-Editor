# Contributing to GM Editor

GM Editor is an experimental open-source preview for campaign
preparation and local player presentation. See [licensing](LICENSING.md) and the
[public preview guide](docs/open-source/README.md).

## Useful contributions

Start with reproducible bugs, confusing workflows, translations, documentation,
and small improvements to preparing and presenting a session. Describe the user
problem before proposing a large feature. Multiplayer and a full VTT are outside
the first preview's scope. Contributions and support do not guarantee a release
date or feature implementation.

Use the bug-report template. Include version/commit, Windows version, language,
display scaling, steps, expected behavior and actual behavior. Use a fictional
campaign to reproduce a problem. Do not post private campaign databases, personal
paths, access tokens or other players' information.

## Build and check

Follow [BUILDING.md](docs/open-source/BUILDING.md). Work in a separate branch and
keep the change focused. Preserve saved-campaign compatibility, stable identifiers
and migration paths. Test changes to persistence, imports or player visibility
against their relevant regression tests. UI changes need a real walkthrough in
English and German at the minimum supported layout, with readable controls and
keyboard focus.

In the pull request, explain the user-visible change, checks actually run, and
remaining limitations. Do not describe an unrun test as passing. For a ticket,
follow [the ticket workflow](docs/GITHUB_TICKETS.md).

## Bounded contributor tasks

Coordinate a task with the maintainer and keep each PR limited to one outcome.
The [preview scope](docs/open-source/README.md) describes the supported direction.
This list is a set of reviewable follow-ups, not claims that the workflows have
passed human or release acceptance. Coordinate priorities with the maintainer.

| Task | Scope and starting point | Done when |
| --- | --- | --- |
| Responsive campaign actions | Measure the remaining synchronous campaign-text capture, player confirmation and durable save paths. Background disk work and image decoding are already implemented; preserve review and persistence guarantees. | Record cold-action timings on identified hardware, reduce a measured main-thread stall, and retain stale-review refusal, cancellation and failure recovery tests. |
| One keyboard/DPI workflow | Pick one of outcome capture, review or next-session selection. Fix focus order and clipped EN/DE text in that panel only. | A reviewer completes the selected flow with the keyboard at the minimum viewport and physical 150% scaling; evidence identifies executable, locale and framebuffer. Do not call simulated scale physical-DPI acceptance. |
| Named UVTT importer fixture | Take one small tactical fixture with a visible grid, one wall and an open/closed door. Record a specific importer, application version and plugin version. | Compare exported image/grid alignment and door behavior against the source, document unsupported semantics, and preserve an importer result fixture or reviewer evidence. No general compatibility claim follows from one importer. |
| Rules calculation example | Choose one existing mixed-SRD calculation or override, such as hit-point estimate or spell-slot maximum. Preserve stored ruleset IDs and pins. | Expected values and cited source coverage are explicit, a regression checks the representative case, and the displayed explanation states the limit of the assistance. Avoid broad edition rewrites. |
| One migration/recovery fault | Extend an existing migration, export or draft-recovery test with one realistic denied-write/locked-file case. Use disposable fixtures. | Failure leaves prior durable bytes and retained draft intact, retry works, and the test checks the concrete invariant rather than implementation detail. |
| Novice wording correction | Observe one blocker in the current guided starter using the [human protocol](docs/open-source/HUMAN_VALIDATION.md); adjust only the affected text/navigation hint. | The issue contains the observed confusion and a revised EN/DE task result; no invented participant or retroactive timing claim. |

Tests must not start the user's desktop application, move their pointer, or read
personal campaign data unless the person has explicitly agreed to that session.
Hosted portable tests and deliberate native/human work remain separate evidence.

## Rights and review

Submit your own contributions under GPL-3.0-or-later and preserve upstream notices.
List the source, license and modifications for any third-party material. Disclose
AI-assisted work and verify its correctness and provenance as you would any other
contribution. Generated content is not automatically cleared for redistribution.

Do not include generated builds, databases, settings, private reviews, payment
details or credentials. Run the release audit when changing assets or packaging.
Only maintainers prepare published releases. No personal self-hosted runner should
execute an untrusted contribution.

Be respectful and specific in reviews. Report security issues using
[SECURITY.md](SECURITY.md), not a public issue containing exploit details or data.
