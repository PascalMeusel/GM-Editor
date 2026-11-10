# Preview limitations

- Interactive multiplayer is not implemented. The player window is local and can
  be shared through a separate screen-sharing application.
- World/region and outdoor tactical maps are the main generation scope. Other
  map generators are experimental.
- The 1 October native Windows walkthrough checked the three-map comparison,
  enlarged candidates, creation of a selected detail map, and atlas fitting at
  150% application text size in a 1280 x 720 client area. It also checked audio
  add/search/play/pause/resume/stop, character creation, scene navigation,
  initiative and combat turns, player review/open black/show/blackout, and
  save/restart with the player window remaining closed. This is a focused local
  check, not the complete sharing or device matrix.
- Real human testing over repeated campaigns remains incomplete. The previous
  simulated-player exercise was an AI simulation, not customer research.
- Specialist translations, high-DPI layouts and keyboard-only flows need further
  review. Report the language, window size and display scaling with UI issues.
- Some rules and resource transactions require GM adjudication. Imported content
  is not automatically licensed for redistribution.
- The preview deliberately selects current map resources, eleven portrait species
  in two collections, and 69 audio files. Other private artwork and legacy packs
  are excluded. Imported campaign identifiers are preserved, but omitted artwork
  is unavailable unless supplied separately with permission.
- The built-in rules mix selected SRD 5.1 and 5.2.1 mechanics and abbreviated
  summaries. They are not a complete or exact implementation of either edition.
- The Windows build is unsigned. A second-machine check and repeated human
  campaign testing remain part of acceptance beyond the local automated checks.
- The October 1 observations predate the connected-session changes. On October 4,
  an AI reviewer performed an independent first-use walkthrough on fictional data.
  A later native retest checked new-place selection, retained World edits across
  navigation, save/reopen and Save and Continue in English, including a 1282x752
  window. A subsequent identified retest checked English/German compact menu
  readability and scene ordering, the combat shortcut opening, maximized
  damage/undo/redo, and stable World-note typing after review/save/reopen. Compact
  combat numeric typing was inconclusive because the automation tool activated
  the owner instead of its separate transient popup. Native resource corrections
  were not tested. These observations do not cover every outcome/recovery route,
  real Windows DPI settings, or the eventual exact release package. Session
  start/report identity and completed-recap fixes also passed a later native
  connected-session walkthrough: explicit start/cancel, historical recap viewing,
  finish/readback, one reviewed place-note consequence and one selected follow-up
  scene persisted after reopen. An independent read-only database/ledger check
  confirmed the typed values and one-time application. Compact outcome typing
  remained inconclusive because the automation tool activated the popup owner;
  maximized entry worked. This is agent observation, not human acceptance.
- October 4 synthetic native measurements used three 80x80 map workloads and
  three 300-frame Player Display runs with a 4096x4096 image. The largest per-phase
  map p95 values were 18.63-19.40 ms; published Player Display p95 was 17.59-17.77 ms. These are
  earlier-build, steady-frame observations on a Ryzen 7 3700X / GeForce RTX 2080
  SUPER desktop, not physical input latency or minimum-hardware acceptance.
  Cold player review took about 300 ms, arm/save about 640 ms, publish/save
  905-950 ms and the first submitted image frame 1.30-1.36 seconds. Those cold paths
  exceeded the 100 ms target. Recovery/backups were disabled for this player
  measurement, and actual review popups were not exercised. The associated
  production application's SHA-256 was
  `b27e5a2e4fb5626869e42d3707dce7c00553c3c585e4f734d1383fd543a43d88`.
  The player harness used that build's copied production libraries and a repaired
  synthetic image fixture; its SHA-256 was
  `92fc47eb89a38af79fc4eea27ec029a8a53f9934a81f0f30191c624afe6db8e9`.
- A later October 4 run exercised actual Editor and legacy GM review popups with
  frozen map/image data. All four 300-frame idle groups retained their captures;
  same-size/mtime source replacement kept old preview pixels and refused actual
  confirmation until explicit refresh. With default backups and recovery enabled,
  published-frame p95 was 17.65-17.87 ms and review-idle p95 16.86-16.90 ms.
  Successful arm/save still took 338-392 ms, publish/save 497-525 ms and first
  submitted image frames 902-944 ms; cold responsiveness remains unaccepted.
  Eight saves produced verified pre/post snapshot receipts. A separate run with
  snapshots disabled also passed but does not establish precise backup overhead.
  The application SHA-256 was
  `2762f1ee1fbb751ef99c540b51aa4f07c333a5515b05355d49b4fd383f092089`;
  the independently repaired test harness SHA-256 was
  `79c4c37adbeceb3e556c19ab2659a91477dd833864e547d810dfda128a90a7a3`.
  Both runs used copied source-bound production libraries and selected resources;
  later candidates require their own checks. The legacy panel
  ran in a test host, and overlapping review/recap texture checks were cache-owner
  probes rather than full concurrent UI walkthroughs.
- Authored Place, Nation and Shop drafts now have separate local recovery
  checkpoints on a two-second cadence. Restore opens them for review; explicit
  Save still commits campaign changes. A synthetic native October 4 check
  interrupted the application after verifying a completed Place-note checkpoint,
  then restored, saved and reopened the exact note. Read-only database checks
  confirmed restoration alone did not save it. One compact transient-popup
  click was inconclusive with the desktop automation tool; maximized restore
  worked. German compact Discard and reopen preserved the original note in a
  separate interrupted-state copy. Compact native EN/DE checks also verified
  the sidebar's pending-recovery cue, safe hover guidance and clearing after
  discard. Failure and post-save cleanup cue states have automated coverage.
  This does not establish power-loss or last-keystroke durability.
  Creation forms, dungeon drafts and transaction confirmation state are outside
  this recovery contract. Session text recovery is a separate facility.
- Undo covers scene ordering/links, objective/clue edits and manual HP, temporary
  HP, resource and token corrections. It guards against changed context and does
  not undo arbitrary scene creation, compound attacks or spells.
- GM Markdown export contains private notes. Import preserves literal UTF-8
  prose as private story-thread notes; it does not reconstruct entity links or
  synchronize another notes application. Editable companions support reviewed
  updates of existing story-thread GM text only; conflicts are never overwritten.
  Disk work is cancellable and runs in the background, but campaign-text capture
  and the final save remain synchronous. Attachments default to none selected.
  See [the exchange contract](../MARKDOWN_DATA_EXCHANGE.md) for limits and scope.
- Player recaps require separate public text and an explicit review. Attachments
  are limited to selected already-public managed images; arbitrary files and PDF
  output are outside this preview.
- Universal VTT output is a bounded tactical image/grid/wall/closed-door transfer
  for the GM, including unexplored image areas. It omits fog state, tokens, labels,
  lights, elevation and movement rules. Sight-only walls can block movement in
  the importer and require adjustment. A headless contract check against Universal
  Battlemap Importer 6.1.1 is not live Foundry rendering acceptance.
- Injected filesystem errors and deterministic interrupted-export states verify
  recoverability paths. Real power-loss durability is not established by those
  tests.

The initial release will be labelled a preview. Known problems should be described
alongside the exact release version and should not be hidden by a passing test count.
