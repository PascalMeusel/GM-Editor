# GitHub tickets for GM Editor

## Capture work while using the program

Bookmark [New ticket](https://github.com/PascalMeusel/GM-Editor/issues/new/choose) and [Open tickets](https://github.com/PascalMeusel/GM-Editor/issues).

Choose **Bug report** for broken behavior or **Work package** for an improvement. English and German are both fine. Describe the desired result, add reproduction steps for bugs, and paste screenshots directly into the description. One independently reviewable outcome per ticket keeps agent work manageable. A blank issue remains available for quick notes; clarify the outcome before implementation.

## Ask an agent to solve a ticket

In Codex, select this repository and send:

> Solve https://github.com/PascalMeusel/GM-Editor/issues/NUMBER. Read the issue and comments, follow AGENTS.md and docs/GITHUB_TICKETS.md, implement the acceptance criteria, run relevant checks, and create a pull request for my review. Do not merge it.

Replace NUMBER with the actual issue number. You can also say **Solve issue #42 in this repository** when the repository context is already selected.

For multiple independent tickets:

> Work on issues #12, #15 and #18 using separate agents and isolated worktrees, one pull request per ticket. Check dependencies and overlapping files before starting. Run relevant checks and leave the pull requests for my review.

Creating a ticket does not launch an agent automatically. This setup supplies the forms and implementation workflow; it does not install a background runner, configure paid API execution or enable a GitHub-hosted coding-agent integration. The agent needs GitHub access to read the issue and publish its PR. If access is unavailable, provide the ticket text and have it prepare the local changes, reporting the publishing blocker.

The documented OpenAI GitHub integration supports `@codex review` and follow-up tasks in **pull request comments** once configured. Do not assume that commenting `@codex` on an ordinary issue starts implementation. See [OpenAI's GitHub integration documentation](https://learn.chatgpt.com/docs/third-party/github).

## Working agreement

1. **Captured:** An open issue records the observation or idea.
2. **Ready:** The desired result and acceptance criteria are clear enough to implement; dependencies are identified.
3. **In progress:** An agent is working on a scoped branch, normally `codex/issue-N-short-description`. Check for an existing PR or active task before launching another agent on the same issue.
4. **Review:** The PR explains the behavior change, links the ticket and records validation. The issue stays open.
5. **Done:** The maintainer accepts and merges the fix. A PR containing `Closes #N` closes the issue when merged into the default branch. Incomplete work uses `Refs #N`.

These are workflow stages, not automatically synchronized labels or a separate project board. GitHub Issues and linked PRs are the source of truth.

## Implementation and validation

Read the current source, issue comments, repository guidance, CMake presets and CI workflows before choosing an implementation. Work from the correct baseline: cloud agents see pushed code, and isolated worktrees do not automatically contain uncommitted desktop changes.

Keep unrelated cleanup and feature expansion out of the ticket. Preserve campaign data and persistence compatibility. For user-visible changes, consider English/German text, resolution/scaling, and save/reopen behavior where relevant.

Run the checks relevant to the changed behavior. Record exact commands and results; distinguish automated tests from visual/manual checks. If a criterion cannot be verified, state what remains and how the maintainer can check it.

## Pull request handoff

Use a short description such as:

```markdown
Closes #NUMBER

Describe the problem and the resulting behavior.

Validation:
- Commands run and their results.
- Manual steps checked.
- Any remaining manual acceptance or known limitation.
```

Use `Refs #NUMBER` if acceptance criteria remain unimplemented. Review the diff and try the original workflow in the program before merging. Reopen the issue with fresh reproduction steps if the problem returns.
