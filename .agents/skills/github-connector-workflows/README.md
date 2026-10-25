# GitHub Connector Workflows Skill

A reusable agent skill for operating remote GitHub repositories through the connected `@GitHub` app when a local checkout, shell, or direct repository filesystem is unavailable.

## Features

- Connector-first repository, branch, file, commit, issue, and PR operations.
- Atomic multi-file commits through Git blobs, trees, commits, and ref updates.
- GitHub Actions as a bounded remote execution plane.
- Dedicated `workflow-mailbox` branch for discovering workflow run IDs and artifact links without source-branch commits.
- Race-safe concurrent workflow reporting with immutable per-run-attempt records.
- Reusable authoritative repository source snapshot workflow.
- Mandatory detailed success/failure logging and unconditional log artifacts.
- Idempotent unified-patch application with already-applied detection.
- Workflow-run, job-log, and artifact troubleshooting.
- Safe workflow triggering when dispatch is unavailable.
- Stale trigger PR detection and cleanup.
- Common failure modes, permissions pitfalls, and race handling.

## Install

Copy the `github-connector-workflows` directory into the skill directory used by your agent host, for example:

```text
~/.agents/skills/github-connector-workflows/
```

For a repository-local installation:

```text
<repo>/.agents/skills/github-connector-workflows/
```

The host must load `SKILL.md` as the entry point. Exact invocation syntax depends on the host; common forms include `$github-connector-workflows` or an explicit skill reference.

## Contents

```text
github-connector-workflows/
├── SKILL.md
├── README.md
├── references/
│   ├── COMMON_TASKS.md
│   ├── PITFALLS.md
│   ├── TOOL_MAP.md
│   └── WORKFLOW_MAILBOX.md
├── templates/
│   ├── workflow-mailbox-publisher.yml
│   ├── repo-source-snapshot.yml
│   ├── logged-remote-task.yml
│   ├── apply-unified-patch.yml
│   └── PR_BODY.md
└── scripts/
    └── split_patch.py
```

## Recommended use

Load the skill before remote GitHub work involving Actions, large patches, workflow troubleshooting, artifacts, or multi-file commits. Read the repository's own workflow policy first; repository-local policy overrides generic examples in this package.

## Workflow mailbox

For ChatGPT web sessions that can read the repository through the GitHub connector but need a stable workflow-run rendezvous point, install `templates/workflow-mailbox-publisher.yml` in the target repository and have task workflows publish into the dedicated `workflow-mailbox` branch. The workflow creates the orphan branch on first use, with only `.workflow-mailbox/**` and a root `latest.json` in its tree. See `references/WORKFLOW_MAILBOX.md`.

The mailbox records the authoritative `run_id`, exact `source_sha`, run URL, result, artifact IDs, authenticated artifact URLs, REST download URLs, digests, and expiry metadata. Concurrent runs retain per-attempt records and update per-workflow and root `latest.json` pointers using deterministic run ordering rather than completion order.

## Repository source snapshot

`templates/repo-source-snapshot.yml` is included as a common workflow for producing an exact source archive, provenance metadata, checksums, submodule state, and symlink state. It reports the run and artifact through the same repository mailbox.
