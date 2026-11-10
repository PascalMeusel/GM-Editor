# Publishing the first community preview

This is the maintainer's sequence for a reviewed free release. Preparation does
not publish anything automatically. Keep the existing private repository and
personal data intact.

## Source review

1. Choose the final source revision or working snapshot. Record its inventory and
   SHA-256 hashes with `tools/prepare_open_source.py`.
2. Use the owner-selected resource profile in
   `distribution/community-preview-resources.json`. The first selection and
   exclusions are recorded in [the content review](CONTENT_REVIEW.md). Resolve
   newly pending asset/content families before adding files. Approvals in
   `distribution/open-source-decisions.json` must name the exact path, `sha256`,
   `license`, source `evidence`, `reviewer`, and `date`. A changed file requires a
   new decision. Do not approve an unknown source simply to make the check pass.
3. Run an up-to-date dedicated secret scanner on the proposed snapshot. Review
   its findings; never post unredacted scan output. Scan old Git history too if
   any of that history will become public. Prefer a new public repository with
   reviewed source when private history contains campaign data.
4. Run the exporter with `--public` into a new directory. It must reject pending
   decisions, missing production files and detected secret/path problems.
   This technical check does not replace the maintainer's content review.
5. Initialize the pinned upstream submodules and verify a fresh build. Keep their
   notices. Ensure the staged root README and local documentation links work.

The local source-export command is:

```powershell
python tools/prepare_open_source.py --public --report-dir <new-private-audit-directory> --stage <new-source-directory> --resource-profile distribution/community-preview-resources.json --decisions distribution/open-source-decisions.json
```

After building that staged tree with `-Configuration Release -StaticRuntime` and
running its selected checks, `tools/package_community_preview.py` creates paired
Windows/source ZIPs, complete dependency source/notices and checksum manifests:

```powershell
python tools/package_community_preview.py --source <staged-source> --build <its-build-directory> --audit <its-audit-directory> --output <new-output-directory>
```

The tool refuses changed source, mismatched dependencies/build roots and existing
output directories. These are local candidates until the exact package has been
reviewed. It does not publish to GitHub or activate funding.

## GitHub setup

- Create the proposed public repository privately first. Choose its final name
  and update issue/security links before publishing.
- Use the portable hosted workflow only. Never register a personal machine as a
  self-hosted runner on the public repository. Old private workflows are excluded
  by the source exporter, and private-only guards were added in the development
  repository as defense in depth.
- Keep workflow permissions read-only; require review for workflow and release
  changes. Do not grant write tokens or secrets to untrusted pull requests.
- Enable and verify private vulnerability reporting before announcing the repo.
- Set the description, preview status, supported platform, issue templates and
  contribution links. Review public README screenshots separately for personal
  information and rights.
- No `FUNDING.yml` or payment link is included until the owner has accepted the
  provider's applicable liability terms. Donations are not required for release.

## Downloadable preview

Test the exact packaged application from a fresh per-user profile, including save,
reopen, export/import, backup/restore, and explicit player-window publication.
Check the minimum window size, text scaling, English/German labels and hidden GM
content. List untested and experimental features alongside the release.

Provide the corresponding source, build instructions, complete code/dependency
licenses, asset notices and SHA-256 checksums with the binary. Verify the download
on another machine. The current Steam packager is a reference for resource and
data-path handling, not an already-approved GitHub package.

Only after source, assets, settings and the exact release files are reviewed should
the maintainer make the repository public and publish a tagged preview. Keep
release publication separate from every ordinary CI run.
