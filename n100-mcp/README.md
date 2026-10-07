# N100 MCP v0.3

Structured home-server MCP used as a fallback execution target when GitHub-hosted
Actions cannot perform a task that requires private/local files, a real runtime,
or device access.

## Execution policy

1. GitHub-hosted Actions first for reproducible builds, conversion, analysis and static QA.
2. N100 MCP second for local/private assets, real device/runtime access and long-lived work.
3. No generic shell tool is exposed.

## v0.3 capability groups

- scoped filesystem read/write + atomic text replace
- resumable any-size chunk transfer with per-chunk/final SHA-256
- recursive file find, grep and log tail
- public GitHub large-file download with host and hash checks
- Git status/diff/log/fetch/pull/add/commit/push + clone/worktree/reset/clean
- synchronous Python/Node file runners
- persistent asynchronous Python/Node jobs with logs/status/cancel
- ZIP/TAR create/list/safe extract
- DDS header/hash inspection
- image info/diff/crop/contact-sheet helpers
- process list and system resource status

The filesystem root defaults to the OS user's home and can be overridden with
`N100_MCP_ROOT`. The HTTP listener remains localhost-only.

## Run

```bash
npm ci
PORT=8765 N100_MCP_ROOT="$HOME" node server.mjs
```

The Secure MCP Tunnel is expected to publish the local MCP endpoint. Keep each
ChatGPT/OpenAI account on its own Linux user, local port and tunnel credentials.
