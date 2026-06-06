# Music Kendama — Project Conventions

Conventions for working on this repo. Synced via git, so they apply on every machine.

## Machines & accounts

- **Machines:** MacBook Pro (`M5PRO14`, home/primary) + MacBook Air (`M5AIR15`, daily carry).
- **GitHub:** every machine and device authenticates to GitHub as **`milesornish`** — not any other account (e.g. `mornish-droid`). On a new machine: `gh auth login` (or `gh auth switch --user milesornish`), confirm with `gh auth status`.
- **Git commit identity** is always `miles ornish <mornish@gmail.com>` on all machines.

## Setup & parity

- One-time per machine: `./scripts/setup-mac.sh` — installs the CLI toolchain (brew, platformio, libserialport, node, gh), Python deps, sets git identity, and creates config files from templates.
- **Parity check:** `./scripts/setup-mac.sh --verify` prints a stable, sorted snapshot (tool versions, gh account, GUI apps, RME driver/SIP state, repo state). Run on two machines and `diff` the output — only the machine-name line should differ.

## Audio interface (RME Babyface Pro FS)

- Install the **DriverKit driver (v4.27+)**, never the legacy KEXT (v3.35) — the KEXT forces Reduced Security on Apple Silicon. DriverKit needs **no security downgrade; keep SIP enabled.** Approve the extension under System Settings → General → Login Items & Extensions.

## Licensed GUI apps (can't be scripted — install manually)

- **Ableton Live 12 Suite** (bundles Max for Live — no separate M4L install).
- **TouchDesigner** — Apple Silicon build, Non-Commercial license.
- RME driver + **TotalMix** (ships with the DriverKit package; the app is `Totalmix.app`).

## What syncs where

- **git** (this repo): all source, scripts, docs, secrets *templates*, released M4L devices.
- **Google Drive:** Ableton sets, samples, working M4L copies, datasheets, photos, receipts.
- **Machine-local (never syncs):** `secrets.h`, `.env.local`, `.pio/` cache, compiled bridge binary.

See `docs/multi-machine-workflow.md` for the full daily workflow, serial-port handling, and Ableton coordination.

## Commit hygiene (enforced)

A tracked pre-commit hook (`scripts/git-hooks/`, enabled via `core.hooksPath` — `setup-mac.sh`
installs it on each machine) **blocks** any commit that adds a secret-like file (`secrets.h`,
`.env.local`, `*.pem`, `id_rsa`, …), a build artifact (`.pio/`, `*.bin`, the compiled bridge), or
secret-pattern content (private keys, `AKIA…`, `ghp_…`, a hardcoded `password/token = …`). The
`*.template` / `*.example` placeholders are allowed — they're meant to sync. A `pre-push` hook
re-scans the pushed commits as a second gate.

- Genuine false positive → bypass with `git commit --no-verify` (or `git push --no-verify`).
- Validate the hooks anytime: `bash scripts/git-hooks/selftest.sh` (all cases must pass).
- `setup-mac.sh --verify` reports `hooks: scripts/git-hooks`; if it shows `UNSET`, run `setup-mac.sh`.
