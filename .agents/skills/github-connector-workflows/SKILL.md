---
name: github-connector-workflows
description: Use for remote GitHub repository work through the @GitHub connector when a local checkout or direct shell access to the repository is unavailable. Covers repository orientation, atomic file changes, branch and PR operations, GitHub Actions as a remote execution plane, same-repository workflow mailboxes, source snapshots, patch application, workflow logs and artifacts, troubleshooting, retries, and stale trigger cleanup.
---

# GitHub Connector Workflows

## Purpose

Use the connected `@GitHub` app as the primary control plane for a remote repository. When the connector cannot directly perform computation, apply a large change, or run repository tools, use a narrowly scoped GitHub Actions workflow as the remote execution plane and use Actions artifacts as the evidence plane.

This skill is designed for agents that can call connector actions but may not have a local clone, an authenticated shell, or direct filesystem access to the remote repository.

## Core operating model

1. **Connector control plane**
   - Resolve repositories, branches, commits, pull requests, issues, files, and workflow results.
   - Create branches, blobs, trees, commits, files, PRs, comments, and labels.
   - Download job logs and workflow artifacts when the installed connector exposes those actions.
2. **Actions execution plane**
   - Run compilers, tests, benchmarks, patch application, repository scripts, and content verification inside GitHub-hosted runners.
   - Keep each workflow bounded to one declared purpose.
3. **Artifact evidence plane**
   - Upload detailed activity logs on success and failure.
   - Upload build, test, benchmark, patch, snapshot, and verification outputs separately from logs.
   - Diagnose failed workflows from the uploaded detailed log artifact, not only from abbreviated step summaries.
4. **Same-repository mailbox discovery plane**
   - Publish run metadata only on the dedicated `workflow-mailbox` branch, automatically created as an orphan if absent.
   - Keep its tree limited to `.workflow-mailbox/**` and root `latest.json`; retain per-key latest and per-run-attempt records.
   - Report run URLs and artifact IDs/download URLs so a later ChatGPT web session can recover the authoritative run through the GitHub connector without relying on container networking.

## Mandatory workflow policy

Every workflow created or modified under this skill must:

1. Initialize a persistent activity log before checkout or other fallible work.
2. Capture tool versions, exact event/ref/source metadata, command output, exit codes, final repository status, and relevant resource usage.
3. Redirect task output to both the Actions console and the persistent activity log.
4. Use an `EXIT` trap or equivalent so failure context is recorded.
5. Upload a dedicated log artifact under `if: always()` with `if-no-files-found: error`.
6. Keep the log artifact separate from successful build or result artifacts.
7. Avoid printing tokens, secrets, credentials, authenticated URLs, or secret-bearing arguments.
8. Never modify `.github/workflows/**` from inside a workflow. Workflow files are changed only through the connector or another explicitly authenticated client outside Actions.
9. Preserve execution boundaries. A compile-only workflow must not execute tests, benchmarks, custom inputs, help/list commands, discovery commands, or compiled project binaries.
10. For connector-driven workflows that must be discoverable from a later ChatGPT web session, publish a final same-repository mailbox record through `templates/workflow-mailbox-publisher.yml`.
11. Upload result and log artifacts before mailbox publication so the mailbox can report their IDs, authenticated artifact URLs, REST download URLs, digests, and expiry metadata.
12. Preserve the exact task/source commit separately from the `workflow-mailbox` commit; never use the mailbox branch head as the source SHA. The source branch must remain unchanged by mailbox publication.
13. Keep all workflow mailbox data exclusively in `workflow-mailbox`; overrides to other branches must be rejected, and its tree is limited to `.workflow-mailbox/**` plus root `latest.json`.

Use `templates/logged-remote-task.yml` as the baseline and `templates/workflow-mailbox-publisher.yml` as the mailbox publisher.

## Start every task by resolving authority

Before reading or writing:

1. Identify `repository_full_name` as `owner/repo`.
2. Resolve the authoritative branch, tag, commit, PR, or issue.
3. For a PR, call `get_pr_info` or `fetch_pr` and record:
   - base branch and SHA;
   - head branch and SHA;
   - state, draft status, and merge status.
4. For branch work, resolve the current head commit before creating dependent commits.
5. Treat branch heads as moving. Re-read the head before a final `update_ref` or other compare-and-swap mutation.
6. Do not infer that a file exists in a local sandbox because the connector returned a file reference.

## Route by task type

| Task | Preferred connector path | Remote workflow needed? |
|---|---|---:|
| Read repository/file/commit/PR state | `get_repo`, `fetch_file`, `fetch_commit`, `get_pr_info` | No |
| Update one small text file | `fetch_file` → `update_file` | No |
| Create one small text file | `create_file` | No |
| Atomic multi-file text update | `create_blob` → `create_tree` → `create_commit` → `update_ref` | No |
| Create branch | `create_branch` | No |
| Open or update draft PR | `create_pull_request`, `update_pull_request`, `convert_pull_request_to_draft` | No |
| Discover latest connector workflow run | Read `.workflow-mailbox/<workflow-key>/latest.json` on `workflow-mailbox`, then use its `run_id` | No |
| Inspect workflow runs/jobs/logs/artifacts | `fetch_commit_workflow_runs`, `fetch_workflow_run_jobs`, `fetch_workflow_job_logs`, `fetch_workflow_run_artifacts` | No |
| Re-run failed jobs | `rerun_failed_workflow_run_jobs` or `rerun_workflow_job` | No |
| Capture an authoritative source snapshot | `templates/repo-source-snapshot.yml` | Yes |
| Compile, test, benchmark, execute repo tools | Create/trigger bounded workflow | Yes |
| Apply a large unified patch | Commit patch parts + trigger patch workflow | Usually |
| Transfer large binary/generated outputs | Workflow artifact | Usually |
| Verify an already-applied patch | Blob/hash comparison first; workflow only if computation is needed | Sometimes |

If an action is not loaded, discover the current connector schema before claiming it is unavailable. Use `gh` only when the connector lacks the required capability and an authenticated CLI environment actually exists.

## Common task procedures

### Read a file safely

1. Call `fetch_file` with explicit repository, path, and ref.
2. Record the returned blob SHA.
3. Use line ranges for large files when only a section is needed.
4. Re-fetch before a write if another actor may have changed the same path.

### Update one file

1. `fetch_file` on the target branch.
2. Prepare the complete replacement content.
3. `update_file` with the returned blob SHA and explicit branch.
4. Verify the resulting commit and re-fetch the file when correctness matters.

A `409` usually means the supplied blob SHA is stale or the same path was changed after it was fetched. Re-fetch that path and retry intentionally. Do not blindly repeat the same request.

### Create an atomic multi-file commit

Use Git data objects instead of sequential `update_file` calls when all files must land together.

1. Resolve the parent commit and its tree.
2. Create one blob per new file content using `create_blob`.
3. Create a new tree using `create_tree` with the parent tree as `base_tree_sha`.
4. Create a commit with `create_commit` and the resolved parent SHA.
5. Move the branch with non-forced `update_ref`.
6. If the ref moved, stop, re-read the head, reconstruct the tree on the new parent, and retry. Never force-update a shared branch merely to bypass a race.

Use this method for related source, test, and documentation edits that must remain coherent.

### Create a branch and draft PR

1. Resolve the exact base branch or commit.
2. `create_branch` from the explicit base.
3. Apply changes to that branch.
4. Verify the branch head.
5. `create_pull_request` with explicit head, base, title, body, and `draft: true` unless the user requests otherwise.
6. Re-fetch PR metadata and report the final head SHA and state.

Do not open a second PR when an existing PR already tracks the work branch. Update the existing PR instead.

### Create or modify a workflow

1. Read repository workflow policy and existing related workflows.
2. Create or update the workflow only through the connector or an authenticated external client.
3. Use least-privilege `permissions`.
4. Use a narrow trigger:
   - `workflow_dispatch` when a human or API dispatch is available; or
   - an exact unique push-marker path when connector dispatch is unavailable.
5. Add task-level `concurrency` only when overlapping task execution itself is invalid. Do not use concurrency as mailbox synchronization; the mailbox publisher is race-safe and preserves concurrent run records.
6. Initialize `/tmp` or `$RUNNER_TEMP` logging before checkout.
7. Upload logs with `if: always()`.
8. Ensure task outputs are uploaded separately and only when valid.
9. Ensure `.github/workflows/workflow-mailbox-publisher.yml` exists when mailbox publication is required, and give the caller `actions: read` plus `contents: write`. The publisher automatically creates the isolated `workflow-mailbox` branch; do not create it from a source branch.
10. Add a final `if: always()` mailbox job after artifact-producing jobs. Use a stable `workflow_key` and pass the exact source SHA/ref plus the caller result.
11. Avoid broad `paths` patterns that allow unrelated documentation or mailbox commits to retrigger the workflow.
12. Do not let a workflow commit changes to its own workflow file.

### Trigger a workflow when dispatch is unavailable

Use a unique marker file such as:

```text
.agents/connector-triggers/apply-patch-<unique-id>.txt
```

Configure the workflow `push.paths` to match only that exact marker or a tightly scoped marker directory. Create the marker once through the connector. The workflow's output commit must not change the marker path, preventing a loop.

Prefer one marker per intended run. Do not keep adding trigger files to a temporary PR after the task is complete. Close stale trigger-only PRs rather than merging them.


### Discover a workflow through the same-repository mailbox

Use `references/WORKFLOW_MAILBOX.md` as the protocol authority.

1. Select the dedicated `workflow-mailbox` branch, never the source branch.
2. Fetch `.workflow-mailbox/<workflow-key>/latest.json`.
3. Record `run_id`, `run_attempt`, `source_sha`, `source_ref`, `result`, and `run_url`. Read root `latest.json` on the same branch when the newest run across all workflow keys is required.
4. Treat `.workflow-mailbox/<workflow-key>/runs/<run-id>-attempt-<attempt>.json` as the immutable record for that attempt.
5. Use artifact IDs with connector artifact actions when available; surface `artifact_url` as the authenticated browser/download link.
6. Never infer success from the presence of a mailbox record; check `result`, jobs, logs, artifacts, digests, and task-specific evidence.

Concurrent executions are expected. Each run attempt owns a unique immutable record. `latest.json` is updated by numeric `(run_id, run_attempt)`, so a slower older run cannot overwrite a newer run merely because it finished later. The publisher retries non-fast-forward pushes on the newest `workflow-mailbox` branch head.

### Capture an authoritative repository source snapshot

Use `templates/repo-source-snapshot.yml` for a connector-readable, artifact-backed source snapshot.

1. Install `templates/workflow-mailbox-publisher.yml` and `templates/repo-source-snapshot.yml` under `.github/workflows/` through the connector.
2. Dispatch the snapshot workflow or call it from another workflow with an exact `source_ref` when authority must be pinned.
3. Record the returned `snapshot_sha`, artifact name, artifact ID, artifact digest, and artifact URL when called as a reusable workflow.
4. For later ChatGPT retrieval, read `.workflow-mailbox/repo-source-snapshot/latest.json` from `workflow-mailbox` and use its `run_id`, `source_sha`, and artifact metadata.
5. Verify `source.tar.gz.sha256`, `SHA256SUMS`, `SUBMODULES.txt`, `SYMLINKS.tsv`, and `snapshot-metadata.json` before treating the snapshot as authoritative evidence.
6. Keep snapshot generation read-only with respect to source content; only the isolated `workflow-mailbox` branch may receive the mailbox publication commit.

### Monitor a workflow

1. Record the event commit SHA when triggering.
2. If the workflow publishes a mailbox, fetch its stable `latest.json` path first and use the reported `run_id`; otherwise use `fetch_commit_workflow_runs` or the most specific available run lookup.
3. Fetch jobs with `fetch_workflow_run_jobs`.
4. Fetch detailed job logs with `fetch_workflow_job_logs`.
5. Fetch artifact metadata with `fetch_workflow_run_artifacts` and compare it to mailbox artifact metadata.
6. Download both:
   - the dedicated log artifact;
   - the successful result/build artifact, when present.
7. Verify artifact digests, internal checksums, exact source commit, source status, and fixture/input closure before executing packaged binaries.

A successful workflow step summary is not enough to close a gate. Verify the artifacts and exact source authority.

### Re-run a failed workflow

1. Read the detailed log artifact first.
2. Classify the failure as:
   - workflow syntax/configuration;
   - checkout/ref race;
   - permissions/authentication;
   - environment/dependency;
   - deterministic implementation failure;
   - transient infrastructure failure.
3. Fix deterministic workflow defects before retrying.
4. Use `rerun_workflow_job` for one isolated failed job or `rerun_failed_workflow_run_jobs` for all failed jobs.
5. Do not retry a malformed workflow; it must be committed in a valid form first.

### Apply a large unified patch through Actions

Use `templates/apply-unified-patch.yml`.

1. Ensure the patch contains no `.github/workflows/**` changes.
2. Split the patch into connector-sized parts if necessary.
3. Commit:
   - ordered patch parts;
   - full patch SHA-256;
   - expected output blob manifest;
   - one unique trigger marker.
4. The workflow must:
   - concatenate parts in deterministic order;
   - verify the full patch digest;
   - reject workflow-file paths;
   - compare current output blob hashes with the expected manifest;
   - return success without applying when every output already matches;
   - run `git apply --check` before applying;
   - verify every output blob after applying;
   - commit and push only the intended result;
   - upload detailed logs unconditionally.
5. After success, compare the remote target blobs to the expected manifest through the connector.

A normal `git apply` failure does not prove the patch is missing. It may mean the patch was already applied or the base changed. Check expected output blobs first. `git apply --reverse --check` can support an already-applied diagnosis, but exact output hashes are authoritative.

### Verify whether a patch was already applied

1. Obtain the expected final Git blob SHA for every target path.
2. Fetch each current target file/blob from the branch.
3. Compare all current blob SHAs to expected values.
4. If every path matches, classify the patch as already present and do not apply it again.
5. If only some match, classify it as a partial or divergent state and investigate; do not force-apply.
6. Record the commit that first introduced the matching blobs when provenance matters.

### Close a stale workflow-trigger PR

A trigger PR is stale when:

- the intended source patch already landed elsewhere;
- its diff contains only markers, temporary workflow triggers, or no-op files;
- merging would add no required implementation state;
- leaving it open can continue to trigger workflows.

Procedure:

1. Fetch PR metadata and changed filenames.
2. Verify the intended output exists on the authoritative work branch.
3. Update the PR body/comment with the evidence.
4. Close without merging.
5. Leave the implementation PR open when it still owns active work.

## Failure and safety rules

- Never treat positional proximity as proof that two remote files, patches, or outputs are equivalent; compare hashes or canonical content.
- Never synthesize a successful workflow result when a command failed.
- Never weaken tests or validation merely to produce a green workflow.
- Never expose secrets through `set -x`, environment dumps, command arguments, URLs, or uploaded logs.
- Never use `pull_request_target` to check out and execute untrusted PR code under elevated permissions.
- Workflows from forks may receive read-only tokens and no secrets. Plan write operations accordingly.
- `GITHUB_TOKEN` permissions are explicit. Set `contents: write` only for workflows that must commit, including same-repository mailbox publication.
- Mailbox publication can fail when branch rules prohibit creating/updating `workflow-mailbox`, or on fork PRs. Do not weaken repository protection; use connector run discovery instead.
- Do not serialize all workflows merely to protect `latest.json`. Preserve per-run immutable records and use the race-safe mailbox publisher.
- Do not use broad force pushes. Prefer compare-and-swap ref updates and explicit branch authority.
- Keep generated logs outside the repository worktree so clean-source checks are meaningful.
- Keep successful result artifacts and unconditional diagnostic logs separate.

## Required completion report

End each task with:

- repository and authoritative ref;
- exact starting and ending commit SHAs;
- files, branches, PRs, comments, or labels changed;
- workflow run/job IDs and run URL when used;
- mailbox branch/path, immutable run-record path, and mailbox commit when used;
- result and log artifact IDs, names, authenticated download URLs, digests, and retention when available;
- checks actually executed and checks deliberately not executed;
- remaining risks or open gates;
- confirmation that temporary trigger PRs/workflows are closed or intentionally retained.

## Supporting material

- `references/COMMON_TASKS.md` — connector call sequences and detailed recipes.
- `references/PITFALLS.md` — failure modes learned from remote workflow operation.
- `references/TOOL_MAP.md` — action selection guide.
- `references/WORKFLOW_MAILBOX.md` — same-repository mailbox schema, concurrency, retrieval, and failure semantics.
- `templates/workflow-mailbox-publisher.yml` — reusable race-safe mailbox publisher.
- `templates/repo-source-snapshot.yml` — reusable authoritative source snapshot workflow with mailbox reporting.
- `templates/logged-remote-task.yml` — mandatory logging baseline with mailbox reporting.
- `templates/apply-unified-patch.yml` — idempotent patch application workflow with mailbox reporting.
- `templates/PR_BODY.md` — remote-work PR evidence template.
- `scripts/split_patch.py` — deterministic patch chunking and digest generation.
