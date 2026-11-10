# Connected-session human validation

Dated protocol: 3 October 2026; evidence-status update: 4 October 2026.

On 4 October, a fresh-context AI reviewer used an isolated Windows preview with
only a short product introduction. It found unclear session menus, World edit
retention problems, a combat shortcut that did not open, and mismatched session
start/report labels. Focused native retests subsequently checked compact EN/DE
menus, World edit/save/reopen and maximized combat corrections. Session identity
and completed-recap changes have separate automated checks and native review.
These results are recorded separately from that original walkthrough. This is agent usability evidence,
not a human participant in G1, G2, G3 or G5.

A later isolated native check recovered one authored World Place note after a
completed checkpoint and deliberate process interruption, then saved and reopened
it. A separate German compact Discard branch preserved the original note.
Read-only database and checkpoint checks support those observations. This is
neither power-loss evidence nor the second-PC/device acceptance required by G6.

Identified earlier builds also completed synthetic native 80x80 map and Player
Display frame measurements. Steady workloads met their frame targets; cold
review and publication exceeded the interaction-blocking target. Those results
do not establish G4 for every interaction or for the final packaged executable.
Physical second-PC, monitor/DPI/audio acceptance and recurring human sessions
remain pending. See [preview limitations](KNOWN_LIMITATIONS.md) for the scope.

This protocol evaluates the connected-session community preview. It has not
been performed by writing this document. Never operate a participant's desktop
while they are using it without their explicit agreement. Record human,
native-rendering and automated outcomes separately.

Before a run, record source revision or a complete dirty-source manifest,
executable/resource SHA-256 values, package version, Windows version, CPU/GPU/RAM,
driver, screen count/resolution, physical DPI scale, locale and storage type.
Use fictional campaign content and one fresh data directory per tester. Provide
the same short introduction to every tester; record every subsequent hint and
restart. Do not silently restart a failed attempt or omit participants who stop.

| Gate | Procedure | Retained result |
| --- | --- | --- |
| G1 first use | Recruit at least five GMs unfamiliar with this application. Start with the packaged guided starter. Ask each to create/edit/export one map, then prepare three scenes with a person/place link and encounter choice. Give no task-specific coaching during timing. | Record map time from task presentation to saved export, scene-preparation time to a saved three-scene plan, help requests, failed actions and completion. At least four of five must meet 10 and 30 minutes respectively. |
| G2 connected sessions | In a running first session, ask the GM to record a decision and a private proposed consequence affecting Mara or a place; review before/after values, reject one proposal, apply another, then deliberately carry a hook and chosen scene into a new second session. | Time capture from deciding to record to saved outcome (15-second target), and next preparation from opening wrap-up to a usable saved next-session plan (5-minute target). Reopen both sessions; verify the first session's source note, applied state and second-session provenance. |
| C4 player boundary | Put distinct private canaries in source notes, entity names/secrets and handout GM notes. Select explicit public text and one already-public image. Inspect Markdown, printable HTML and every attachment. Revoke the image and change public text, then try exporting the old review. | The package contains only reviewed text/selected bytes; stale/revoked review refuses export; earlier packages remain unchanged. Record what the player physically saw, not just model checks. |
| D exchange | Export GM Markdown with one chosen attachment, inspect the stable-ID index and import one UTF-8 file through the literal preview. Export a tactical UVTT fixture and import it into a specifically named application/importer version. | Original Markdown bytes survive save/reopen, no external Markdown links/files are read, and existing notes remain. Record UVTT image/grid/wall/door observations and all unsupported semantics. A format test alone does not pass importer acceptance. |
| G3 recurring use | Invite ten GMs to run two real sessions each. Then follow five continuing GMs over eight weeks, with a brief weekly record of preparation, actual use, workarounds and reasons for missed sessions. | Retain attendance, sessions run, failures/data recovery, what they used instead, and whether they chose to return. Report dropouts and missing weeks. Synthetic agents or demonstrations do not count. |
| G4 responsiveness | Use a fixed seeded 80×80 fixture on named target hardware. Record warm-up separately, then frame durations and main-thread blocking during map pan/zoom, selection, next-turn, outcome capture and exchange actions. Include expensive actions and any disk stalls in the observations. | Provide raw samples, p50/p95/max, workload and measurement method. Target p95 frame ≤33 ms and interaction blocking ≤100 ms. Do not substitute mean FPS. Record synchronous Markdown inventory/export stalls as failures if they exceed the target. |
| G5 practical advantage | Give each tester two equivalent connected-session scenarios, one in their normal tool/workflow and one here. Counterbalance the order across testers and use distinct but matched content to reduce learning advantage. Include setup/training time separately and apply the same help policy. | Keep paired preparation times and completion-quality checks. Compute each tester's change and the median paired percentage reduction; target at least 25%. Name the compared workflows/versions, sample size, failures and uncertainty. Do not claim superiority from an unpaired demo. |
| G6 devices/recovery | On a second PC with the exact package, test startup, save/reopen, clone import, package transfer and restore. Use real audio hardware and a physical player display for publication/blackout, disconnect/reconnect, DPI and restart. | Retain package hashes, machine/device details and named human observations; the restart is silent and player display remains black until deliberate review/publication. Keep unavailable devices pending. |

Each result row needs a participant pseudonym, task ID, start/end UTC times,
elapsed seconds, locale, pass/fail/pending, help given, defect links and evidence
paths/hashes. Obtain permission before recording screens or voices and use only
fictional examples in shared reports. A discovered data-loss or private-player
leak stops the affected trial; preserve the fixture and file a focused defect.
After a fix, rerun the affected task on the new identified candidate rather than
rewriting the previous result. Public release remains a separate maintainer
decision; a source implementation or protocol document does not accept a gate.
