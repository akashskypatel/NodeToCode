# Summary

Describe the remote repository change and why it is needed.

## Authority

- Repository: `OWNER/REPO`
- Base: `BASE_BRANCH` at `BASE_SHA`
- Head: `HEAD_BRANCH` at `HEAD_SHA`
- Execution policy: `compile-only`, `test-only`, `patch-only`, or other explicit boundary

## Changes

- File or subsystem change 1
- File or subsystem change 2

## GitHub Actions evidence

- Workflow: `PATH`
- Run ID: `RUN_ID`
- Job ID: `JOB_ID`
- Conclusion: `CONCLUSION`
- Exact executed source: `SHA`

### Result artifact

- ID/name: `...`
- SHA-256: `...`
- Internal checksums: `N/N`

### Detailed log artifact

- ID/name: `...`
- SHA-256: `...`
- Uploaded under `if: always()`: yes

## Validation

State exactly what ran and what did not run.

## Remaining risk

List open gates, unverified platforms, or deferred runtime work.

## Temporary workflow/trigger disposition

State whether trigger markers, temporary workflows, and trigger-only PRs were removed, closed, or intentionally retained.
