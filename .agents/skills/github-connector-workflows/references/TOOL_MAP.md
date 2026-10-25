# @GitHub Connector Tool Map

Tool availability may vary. Discover the installed schema before declaring a capability unavailable.

## Orientation and reads

| Need | Action |
|---|---|
| Authenticated identity | `get_profile`, `get_user_login` |
| Repository metadata | `get_repo` |
| Accessible repositories | `list_repositories`, `list_repositories_by_installation` |
| Find repositories | `search_installed_repositories_v2`, `search_repositories` |
| Find branches | `search_branches` |
| Read file | `fetch_file` |
| Read arbitrary GitHub file URL | `fetch` |
| Read blob | `fetch_blob` |
| Read commit | `fetch_commit` |
| Compare refs | `compare_commits` |
| Search code | `search` |
| Search commits | `search_commits` |

## Git data writes

| Need | Action |
|---|---|
| Create branch | `create_branch` |
| Create small text file | `create_file` |
| Replace small text file | `update_file` |
| Delete file | `delete_file` |
| Create content blob | `create_blob` |
| Build atomic tree | `create_tree` |
| Create commit | `create_commit` |
| Move branch ref | `update_ref` |

## Pull requests and issues

| Need | Action |
|---|---|
| PR metadata | `get_pr_info`, `fetch_pr` |
| Changed filenames | `list_pr_changed_filenames` |
| Full or per-file patch | `fetch_pr_patch`, `fetch_pr_file_patch`, `get_pr_diff` |
| Create PR | `create_pull_request` |
| Update/close/reopen PR | `update_pull_request` |
| Draft state | `convert_pull_request_to_draft`, `mark_pull_request_ready_for_review` |
| PR comment | `add_comment_to_issue`, `update_issue_comment` |
| Review threads | `list_pull_request_review_threads`, `reply_to_review_comment`, `resolve_review_thread` |
| Issue create/update | `create_issue`, `update_issue` |
| Labels/assignees | `add_issue_labels`, `remove_issue_label`, `add_issue_assignees` |

## Actions and artifacts

| Need | Action |
|---|---|
| Latest known run via same-repo mailbox | `fetch_file` on `.workflow-mailbox/<workflow-key>/latest.json` at `workflow-mailbox` (root `latest.json` for global latest) |
| Runs associated with commit | `fetch_commit_workflow_runs` |
| Run jobs | `fetch_workflow_run_jobs` |
| Job steps | `fetch_workflow_job_steps` |
| Detailed job logs | `fetch_workflow_job_logs` |
| Run artifacts | `fetch_workflow_run_artifacts` |
| Download artifact | `download_workflow_artifact` |
| Re-run one job | `rerun_workflow_job` |
| Re-run failed jobs | `rerun_failed_workflow_run_jobs` |
| Commit status | `get_commit_combined_status` |

## Selection rules

- Prefer `fetch_file` over generic `fetch` when repository, path, and ref are known.
- Prefer atomic Git data writes for coherent multi-file changes.
- Always fetch workflow mailbox paths from the dedicated `workflow-mailbox` branch, not the source branch; then use connector run/job/artifact actions for verification and download.
- Prefer connector run/job/artifact actions before falling back to `gh`.
- Use mailbox `artifact_url` for a human/browser link, but use artifact IDs with connector download actions when possible.
- Use a remote workflow only for computation or repository operations the connector cannot directly perform.
