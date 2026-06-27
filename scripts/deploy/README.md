# Auto-deploy (pull → build → restart)

Polling-based continuous deployment for the dartserver host. A systemd timer
runs `scripts/deploy.sh` every 60s; if the tracked branch moved on GitHub, it
rebuilds **only** the component that changed (C++ server and/or Next.js web)
and restarts the matching service.

- No inbound ports, no webhook, no GitHub secrets — the server only makes an
  outbound `git fetch`.
- Latency: a push is live within ~1 minute.

## One-time install (on the server)

Run these once on the Ubuntu host, as the repo owner `pstq`.

```bash
cd /home/pstq/github/cam-stream

# 1. Make sure the repo is on the branch you push to, with an upstream set.
git checkout main                    # or whatever branch you deploy
git branch --set-upstream-to=origin/main

# 2. Let pstq restart the two services without a password.
sudo install -m 0440 scripts/deploy/dartserver-deploy.sudoers \
  /etc/sudoers.d/dartserver-deploy
sudo visudo -cf /etc/sudoers.d/dartserver-deploy   # must say "parsed OK"

# 3. Install the timer + oneshot service.
sudo cp scripts/deploy/dartserver-deploy.service /etc/systemd/system/
sudo cp scripts/deploy/dartserver-deploy.timer   /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now dartserver-deploy.timer
```

## Verify

```bash
# First, a manual dry run (also rebuilds+restarts if commits are pending):
scripts/deploy.sh

# Timer status / next run:
systemctl list-timers dartserver-deploy.timer

# Watch a deploy happen live (push something, wait ~1 min):
journalctl -u dartserver-deploy.service -f

# Force a full rebuild + restart of both components:
scripts/deploy.sh --force
```

## How it decides what to rebuild

`git diff --name-only <deployed> <incoming>`:

- a path under `dartserver/` **or `detection/`** → `scripts/build-server.sh
  release` + restart `dartserver.service`. (The server CMake pulls in the
  `camdetect` detection library via `add_subdirectory(../detection)`, so a
  detection change is compiled straight into the `dartserver` binary — no
  separate build step is needed.)
- a path under `GoDartss/` → `scripts/build-web.sh release` + restart
  `dartserver-web.service`
- only docs/other changed → pulls, but restarts nothing.

## Notes / caveats

- **The deploy box mirrors the remote.** `deploy.sh` does `git reset --hard` to
  the upstream commit, so any local edits or commits on the server are
  discarded. Don't hand-edit code on the server — push from your dev machine.
- The service runs under a **login shell** (`bash -lc`) so `pnpm`/`node` from
  nvm/corepack are found. If your Node is system-wide (`/usr/bin/node`) this
  doesn't matter.
- Overlapping runs are prevented two ways: the timer counts 60s from when the
  last run *finished*, and `deploy.sh` takes an exclusive `flock`.
- Change the branch by checking it out on the server (deploy.sh follows
  whatever branch is currently checked out and its upstream).
