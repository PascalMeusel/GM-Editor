# Architecture overview

The C++17 desktop application uses Dear ImGui, GLFW and OpenGL. SQLiteCpp provides
the persistence layer. Application startup and the shared shell live in the root
`Application*` files; `Controllers`, `Models` and `Views` hold the main code.

| Area | Starting point | Responsibility |
|---|---|---|
| Session workflow | `Controllers/Editor/EditorWindowController*` and `Views/Editor/SessionWorkspace*` | Navigation, preparation and live-session context. |
| Persistence | `Models/Project/CampaignRepository*`, `Models/Persistence/Database*` | Loading, saving, migrations and campaign transactions. |
| Maps | `Models/Map/`, map creator and viewer controllers/views | Generation, linked atlas geography, editing, rendering and export. |
| Player presentation | `Models/Session/PlayerSafe*`, `PlayerDisplayReview*`, `PlayerReviewSession*`, local player display controller/view | Capture private review data and explicitly publish a reviewed subset of campaign information. |
| Session outcomes | `Models/Session/SessionOutcomeService*`, `Views/Editor/SessionOutcomes*` | Versioned session-property ledger; staged, idempotent review and guarded undo; explicit next-session provenance. |
| Reviewed recap/asset bytes | `PlayerDisplayPublicationAssets*`, `PlayerRecapPackage*` | Private frozen image copies; render/export public DTOs; stale consent/content invalidates export. |
| Recovery and correction | `TextDraftRecovery*`, `WorldDraftRecovery*`, `CampaignEditHistory*` | Separate unfinished draft checkpoints and stable-ID deltas; failed persistence retains draft/history. |
| Data exchange | `CampaignMarkdownExchange*`, `MapUvttExport*` | Explicit inventories, previewed imports, bounded formats and failure-safe publication. |
| Rules | `Models/Rulesets/` | Compiled fallback rules, definitions, version pins and content attribution. |
| Build and verification | `cmake/`, `scripts/`, `tools/` | Production targets, dependency patches, regression tests and review utilities. |

Persistence and stable IDs are shared across workflows. A view should not create
an independent campaign state or bypass the repository's save/error handling.
World inspector edits retain their original values and proposed field changes by
stable ID. Saving merges only those fields after checking for conflicting live
changes; missing records remain reviewable rather than being recreated silently.
`WorldDraftRecovery` checkpoints the original and edited authored fields of Place,
Nation and Shop drafts. Its bounded, versioned payload uses an independent key in
`Recovery/Drafts`. The editor binds recovery to the current campaign instance and
data root, checkpoints changed drafts on a two-second cadence and checks writes
before leaving. A found checkpoint stays pending until explicit Restore for
review or Discard; Copy preserves its bytes without resolving it. Restoration
does not modify canonical campaign data and retains the original conflict
baselines. Discard updates recovery before dropping RAM
edits; a successful canonical save acknowledges its changes even when subsequent
checkpoint cleanup needs retry. Creation forms, dungeon editors and transaction
confirmation state are outside this checkpoint's contract.
Shared pending undo history belongs to the live Campaign object, and a successful
save acknowledges the corresponding changes across editor and session surfaces.
Player presentation must use the existing filtering and review path; never send a
raw campaign object to a player surface. Maps use a common render plan for previews,
editor output and exports; preserve that relationship when changing art or labels.

Replacing a campaign, including reopening the same campaign ID, revokes pending
map operations and export consent. Obsolete asynchronous work can finish without
blocking the interface, but cannot install its result into the replacement owner.

`cmake/SharedProduction.cmake` defines reusable production boundaries. Dependencies
are pinned and patched in build-local copies by `cmake/PatchedDependencies.cmake`.

Outcomes use the reserved `gm.session.outcomes.v1` property on the persisted
session metadata. The bounded, versioned codec preserves legacy sessions with
no property; malformed/future ledgers are rejected without rewriting them.
Clone import remaps both record IDs and provenance in two passes. No historical
note, map, or campaign ruleset pin is rewritten to introduce this feature.

Campaign export and repository recovery have different contracts: transfer
builds a fresh single-campaign database and an explicit managed-asset inventory;
recovery snapshots can cover the repository and must never be copied into a
transfer package. Unfinished text checkpoints live under `Recovery/Drafts` and
are private. Diagnostic export is an allowlist of counts/version/boolean state,
not a redacted copy of arbitrary logs.

The player display checks source metadata and consent during updates and checks
asset bytes in a background job scheduled every 500 ms. Scan duration adds to
revocation latency. During that interval only the originally reviewed frozen
bytes are renderable. Explicit review/export performs full content verification.
Reviewed texture entries belong to a publication generation. Live cache hits use
that generation's frozen identity without rereading the original asset. Expired
GPU entries are retired after all viewports render and submit their frames, not
from a model destructor or background worker. Initial review, publication and
persistence can still perform synchronous work.

An opened `PlayerReviewSession` owns frozen text, separately filtered GM/player
map projections and captured image assets. Idle preview rendering does not build
another review or read mutable image paths. Conservative invalidation keeps an
old preview visible but requires explicit refresh; it never authorizes publication.
Confirmation still performs canonical pre/post-save validation and detached asset
installation. Owner/context checks prevent a save callback from reviving consent
after campaign replacement, discard or a new review generation. Player-only
validation omits unused GM compilation while retaining the public filtering and
content checks. Private GM artwork is separately owned and cannot enter the
detached display through this adapter.
