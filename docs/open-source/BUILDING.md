# Building GM Editor

The supported application target is Windows x64. Linux core checks are useful to
contributors but do not establish a supported Linux desktop release.

## Windows prerequisites

- Git with submodule support.
- Visual Studio 2022 or Build Tools with Desktop development with C++, an x64
  compiler and Windows SDK.
- CMake 3.21 or newer. The build helper uses Visual Studio's Ninja installation.
- Python 3.10 or newer for the release inventory and its tests.

Use a short checkout path. Git may need `git config core.longpaths true` in this
repository because legacy asset paths are long. Initialize the pinned submodules:

```powershell
git submodule update --init --recursive
```

The paired source ZIP already contains the pinned ImGui, GLFW and SQLiteCpp code.
When building from that ZIP, skip the Git submodule command. CMake uses the
included `_vendor/SQLiteCpp` automatically.

Run from the repository root:

```powershell
.\scripts\build-windows-msvc.ps1 -Configuration Release -StaticRuntime -BuildDir F:/GMEditorBuild/Community -Jobs 4 -Targets TTRPG_Editor
& F:/GMEditorBuild/Community/TTRPG_Editor.exe
```

Replace `F:/GMEditorBuild/Community` with a writable build directory on a drive
with ample free space. Use `-Configuration Release` explicitly. The helper defaults
to Debug. For a Git checkout, the initial configure downloads the pinned SQLiteCpp
revision and needs network access. The paired source ZIP supplies it locally.
Application use does not require a runtime AI account.

The existing full test suite includes legacy artwork and private release-evidence
checks. A restricted public asset selection must document which tests it supports;
do not silently mark missing-content checks as passing. Some archive tests require
at least 50 GiB free on the temporary-files drive; point `TEMP` and `TMP` at a
suitable writable directory for the test process when necessary.

## A smaller regression run

Build the selected test executables before running the focused checks:

```powershell
.\scripts\build-windows-msvc.ps1 -Configuration Release -StaticRuntime -BuildDir F:/GMEditorBuild/Community -Jobs 4 -Targets foundation_tests,schema_migration_tests,domain_persistence_tests,incremental_persistence_tests,player_safety_tests,complete_session_tests
ctest --test-dir F:/GMEditorBuild/Community --output-on-failure --parallel 4 -R '^(foundation_tests|schema_migration_tests|domain_persistence|incremental_persistence|player_safety_tests|complete_session)$'
python -m unittest discover -s tools -p test_prepare_open_source.py
python -m unittest discover -s tools -p test_package_community_preview.py
```

Check the actual registered names with `ctest --test-dir <build> -N`. The current
public CI also runs portable map preview, journal and export checks.

The connected-session regression lane adds export isolation/replacement,
migration safety, bounded autosave, scoped undo, draft recovery, outcomes,
reviewed assets/recaps, Markdown exchange, UVTT and diagnostic privacy. It also
checks campaign replacement, reviewed-image lifetime and production session/map
controls with CPU ImGui input in English and German at normal and compact sizes:

```powershell
$targets = @(
    'campaign_export_tests', 'database_migration_safety_tests', 'autosave_policy_tests',
    'campaign_edit_history_tests', 'tabletop_history_controller_tests',
    'text_draft_recovery_tests', 'session_outcome_tests', 'session_presentation_ui_tests', 'session_plan_tests', 'battle_tracker_turns_tests', 'battle_tracker_preparation_tests',
    'workspace_ui_interaction_tests', 'map_creator_campaign_boundary_tests',
    'world_inspector_draft_tests', 'world_draft_recovery_tests', 'world_inspector_tests', 'world_context_workflow_tests',
    'notification_history_tests',
    'player_publication_assets_tests', 'player_review_session_tests', 'map_player_render_geometry_tests', 'map_preview_parity_tests', 'scoped_image_texture_cache_tests',
    'player_recap_package_tests', 'campaign_markdown_exchange_tests', 'uvtt_export_tests',
    'diagnostic_report_tests', 'travel_session_integration_tests', 'combat_edit_history_tests',
    'starter_campaign_template_tests', 'character_advancement_tests'
)
.\scripts\build-windows-msvc.ps1 -Configuration Release -BuildDir build-ui -Jobs 2 -Targets $targets
ctest --test-dir build-ui --output-on-failure --parallel 2 -R '^(campaign_export|database_migration_safety|autosave_policy|campaign_edit_history|tabletop_history_controller|text_draft_recovery|session_outcome|session_presentation_ui|session_plan_tests|battle_tracker_turns|battle_tracker_preparation|workspace_ui_interaction(_german|_compact|_compact_german)?|map_creator_campaign_boundary|world_inspector_draft|world_draft_recovery|world_inspector_tests|world_context_workflow|notification_history|player_publication_assets|player_review_session|map_player_render_geometry_tests|map_preview_parity_tests|scoped_image_texture_cache|player_recap_package|campaign_markdown_exchange|uvtt_export|diagnostic_report|travel_session_integration|combat_edit_history|starter_campaign_template_tests|character_advancement)$'
```

These tests create isolated synthetic campaigns and do not open application
windows. The Windows and Linux hosted workflow in `portable-core.yml` includes
this lane; a contributor does not need access to a personal self-hosted runner.
Use `--parallel 1` when working on the same PC. First builds can take considerable
time; do not infer a hang from a quiet compilation step. Keep physical displays,
audio, Windows scaling and repeated GM sessions in the separate acceptance lane.

## Source and binaries

`tools/prepare_open_source.py` inventories the current working files, including
intended uncommitted code, and can prepare a **private review snapshot**. It never
copies `.git`, campaign databases, settings, outputs or the commercial planning
folders. It records hashes and unresolved content decisions.

Run `python tools/prepare_open_source.py --help` for arguments. An unresolved
inventory cannot be exported with `--public`. Private review staging is useful for
a fresh build but is not permission to upload its files. Do not distribute a loose
build-tree executable or turn the existing private repository public as a shortcut.
