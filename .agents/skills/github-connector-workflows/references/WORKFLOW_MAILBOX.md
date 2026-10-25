# Same-Repository Workflow Mailbox

Use this protocol when a ChatGPT web session can read repository files through the GitHub connector but cannot discover a workflow run ID reliably enough from the run API alone.

The workflow writes a small JSON record to a dedicated `workflow-mailbox` branch in the same repository, never to the source checkout branch. The connector then reads that stable path to obtain the authoritative workflow run ID, exact source SHA/ref, run URL, result, and artifact metadata.

## Canonical layout

For workflow key `repo-source-snapshot`, the `workflow-mailbox` branch tree is:

```text
latest.json                       # Newest published run across all workflow keys
.workflow-mailbox/
└── repo-source-snapshot/
    ├── latest.json               # Newest run for this workflow key
    └── runs/
        ├── 12345678901-attempt-1.json
        ├── 12345678902-attempt-1.json
        └── 12345678902-attempt-2.json
```

Only `.workflow-mailbox/**` and the root `latest.json` may appear in this branch. The first publication creates `workflow-mailbox` automatically as an **orphan branch** with no source-tree content. A pre-existing branch is reused, with unrelated tracked paths removed from its current tree without force-pushing or rewriting its history. Source files and workflows remain solely on their original branches.

Use only stable keys matching `[A-Za-z0-9._-]+`.

Both latest pointers copy the newest authoritative record according to numeric `(run_id, run_attempt)`, not completion order. The root pointer covers all workflow keys; `.workflow-mailbox/<key>/latest.json` covers that key alone.

## Why both latest and immutable records exist

A single overwritten file is insufficient when workflow executions overlap. An older long-running workflow can finish after a newer workflow and would otherwise overwrite the newer run ID.

The package uses immutable `runs/<run_id>-attempt-<attempt>.json` history plus two latest pointers (per-key and repository-wide), updated only when the candidate `(run_id, run_attempt)` is newer than the current record.

The publisher rebuilds its commit on the current remote branch head after a non-fast-forward rejection and retries. Concurrent runs therefore retain their individual records, while `latest.json` converges deterministically.

A re-run keeps the same `run_id` and increments `run_attempt`; the higher attempt supersedes an earlier attempt of that same run. A later `run_id` remains newer than a re-run of an older run ID.

## Canonical record

A mailbox record contains at least:

```json
{
  "schema_version": 1,
  "workflow_key": "repo-source-snapshot",
  "workflow": "Reusable Repository Source Snapshot",
  "repository": "owner/repo",
  "event": "workflow_dispatch",
  "event_sha": "...",
  "source_sha": "...",
  "source_ref": "refs/heads/dev",
  "run_id": 12345678901,
  "run_attempt": 1,
  "run_number": 42,
  "run_url": "https://github.com/owner/repo/actions/runs/12345678901",
  "result": "success",
  "reported_at": "2026-09-22T16:00:00Z",
  "mailbox_branch": "workflow-mailbox",
  "artifacts": []
}
```

Each artifact entry reports:

- artifact ID and name;
- size;
- digest when GitHub provides one;
- created/expiry timestamps and expired state;
- `artifact_url`, the authenticated GitHub artifact page/download URL;
- `archive_download_url`, the authenticated Actions artifact REST download endpoint.

Artifact URLs are not bearer links. Private-repository downloads still require GitHub authentication. For connector-driven work, prefer the artifact ID with the connector's artifact download action when available; keep the URLs as direct human/browser links and provenance.

## Install the publisher once per repository

Copy:

```text
templates/workflow-mailbox-publisher.yml
```

to:

```text
.github/workflows/workflow-mailbox-publisher.yml
```

Create or update that workflow through the GitHub connector or another authenticated client outside Actions. Do not let a running workflow modify `.github/workflows/**`.

A caller that publishes mailbox state must grant:

```yaml
permissions:
  actions: read
  contents: write
```

Then add a final job:

```yaml
  mailbox:
    if: always()
    needs: [task]
    uses: ./.github/workflows/workflow-mailbox-publisher.yml
    with:
      mailbox_branch: workflow-mailbox
      workflow_key: my-task
      source_sha: ${{ github.sha }}
      source_ref: ${{ github.ref }}
      result: ${{ needs.task.result }}
      summary: Short bounded task description.
```

The task's result/log artifacts must be uploaded before this job starts so artifact discovery can report them.

## Mailbox branch

The publisher always targets `workflow-mailbox`, regardless of the caller's branch, tag, or PR head. The supplied templates pass that value explicitly, and overrides naming another branch are rejected to prevent accidental source-tree deletion. Update any legacy callers that set `mailbox_branch` to a source branch. The publisher creates a missing branch automatically; no manual initialization or placeholder commit is needed.

The workflow must have `contents: write` permission to create and update the branch. Branch rules or fork-PR token restrictions can still deny writes; retain connector run discovery as the fallback. Do not weaken branch protection. Always trust `source_sha` in a mailbox record as the task's source authority, **not** the separate mailbox branch head.

## Trigger-loop safety

Use the repository `GITHUB_TOKEN` for mailbox pushes. Normal events created by a `GITHUB_TOKEN` push do not create another workflow run, which prevents a mailbox commit from recursively triggering ordinary `push` workflows.

Still keep task workflow `paths` filters narrow and do not intentionally trigger on `workflow-mailbox` or `.workflow-mailbox/**`.

## Connector retrieval procedure

Use branch `workflow-mailbox` for all mailbox reads (never the source branch):

1. Fetch `.workflow-mailbox/<workflow-key>/latest.json` from `workflow-mailbox`, or read its root `latest.json` when discovering the newest run across all workflow keys.
2. Record `run_id`, `run_attempt`, `source_sha`, `result`, and `run_url`.
3. Treat the immutable path in `runs/` as the historical record for that run attempt.
4. Use the connector's workflow-run APIs with the retrieved `run_id` when available.
5. Use `artifacts[*].id` for connector artifact download and `artifact_url` for a human/browser download link.
6. Verify artifact digests/internal manifests before using packaged outputs.

Example agent instruction:

```text
Read .workflow-mailbox/repo-source-snapshot/latest.json from branch workflow-mailbox.
Use its run_id as the authoritative GitHub Actions run, source_sha as the
source authority, and artifact IDs/URLs to retrieve the snapshot evidence.
```

## Failure semantics

A mailbox is a discovery aid, not proof that the task succeeded.

- `result` may be `failure`, `cancelled`, or `skipped`.
- A syntax-invalid workflow cannot execute the publisher and therefore cannot create a mailbox record.
- A forcibly cancelled run may not reach the final mailbox job.
- A write-protected `workflow-mailbox` branch or fork PR may prevent publication.
- Artifact expiration invalidates the artifact URLs even though the mailbox record remains.

When the mailbox is absent or stale, fall back to connector run discovery using the known event/source commit and inspect job logs directly.
