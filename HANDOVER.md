# Handover — scx-manager Arch port

## Status

De-brand **COMPLETE and verified**; **AUR publish PENDING** (next cycle).

## What is done

- **De-brand:** all user-visible identity is neutral/Arch — display
  "SCX Scheduler Manager", organization "Arch Linux", org domain
  `archlinux.org`, reverse-DNS `org.archlinux.scx-manager`, neutral
  hicolor icon `scx-manager.svg`, `scx-manager.desktop`; README and
  `configure.sh` de-branded.
- **Clean build:** from-scratch Release build with **0 compiler
  warnings** (scoped pure pragma + `FMT_SYSTEM_HEADERS=ON`); `ldd` shows
  0 missing libraries; install layout resolves under `usr/` (bin,
  desktop, svg, lib, headers, CMake config).
- **Packaging:** release source package `packaging/scx-manager/`
  (`pkgrel=2`, source tarball, `.SRCINFO`; builds at install time) and
  the `-git` template `packaging/scx-manager-git/` (`PKGURL` still a
  `<GIT_REPO_URL>` placeholder pending the public GitHub URL). The
  pre-built `-bin` staging (`aur-publish/`) was deprecated and
  gitignored.
- **UX:** "Cancel" → "Close" button; `.ts` translations regenerated
  (`lupdate -no-obsolete`) with the `Close` entry corrected to clean.
- **Docs:** README now carries the "Available schedulers" section
  (13-row table); new `BLUEPRINT.md`, `README.md`, and `HANDOVER.md`
  committed.

All work is merged to `main` (chunk commits 1/2/4/6/7/8/8b, plus
packaging, `.gitignore` hardening, and this docs commit — see `git log`).

## Key decisions

- **De-brand identity:** neutral "SCX Scheduler Manager" / Arch Linux
  org with `org.archlinux.*` reverse-DNS; no CachyOS branding remains in
  any user-visible surface.
- **`-bin` dropped in favor of build-from-git source:** the public
  source repository is the package source; the pre-built binary variant
  was deprecated and is gitignored (never publish it).
- **GPL satisfied by a public source repo:** the AUR package needs a
  public source, so a **GitHub fork is required** — it is the GPL source
  and what the `-git` package tracks.
- **Warnings made clean:** scoped pure pragma for first-party code plus
  header-only fmt with `FMT_SYSTEM_HEADERS=ON`; the Release build is
  warning-free.
- **`AUR KIT` secured:** the private AUR keys are gitignored, are not in
  git history, and must never be committed or uploaded.
- **Elevation design kept as upstream:** `pkexec` invoked from the Rust
  layer, no in-repo polkit files; `org.scx.Loader` comes from scx-tools.

## Repository layout

```
scx-manager/
├── src/            # C++23/Qt6 app (main, schedext-window, scx_utils)
├── scx-rustlib/    # Rust crate (cxx bridge, zbus/D-Bus client)
├── cmake/          # CMake helpers (CPM, warnings, sanitizers, config)
├── include/        # public headers (scx-manager/)
├── lang/           # Qt translations (.ts)
├── packaging/      # AUR packages: scx-manager/, scx-manager-git/
├── .github/        # CI workflows (build, checks)
├── LICENSE         # GPL-3.0-or-later (retained)
├── README.md       # user-facing README
├── BLUEPRINT.md    # project blueprint
├── HANDOVER.md     # this file
├── CMakeLists.txt  # top-level build
├── configure.sh    # convenience configure wrapper (de-branded)
├── compile_flags.txt
├── scx-manager.desktop
├── scx-manager.svg # neutral hicolor icon
└── scx_manager_locale.qrc
```

**Gitignored (never commit or upload):** `AUR KIT/` (private AUR
ssh+gpg keys), `aur-publish/` (deprecated `-bin` staging), `build/`,
`target/`, packaging build artifacts (`packaging/*/pkg/`,
`packaging/*/src/`, `*.log`, `*.sig`, `*.pkg.tar.*`, source tarballs),
dev state (`DEV_LOG.md`, `MASTER_LOG.md`, `plans/`, `.kilo/`), and
editor/OS files (`.vscode/`, `.idea/`, `*.swp`, `.DS_Store`, …).

## SECURITY (read before any push)

- The **`AUR KIT`** folder at the repo root holds the user's **PRIVATE
  AUR ssh + gpg keys**. It is gitignored, is not in git history, and
  must **NEVER** be committed, staged, or uploaded — not to GitHub, not
  to the AUR, nowhere.
- **Before any push** (GitHub or AUR): run `git status` and
  `git check-ignore`, and review the staged diff — no key material, no
  `aur-publish/` content, and no `*.pem`/`*.asc`/`id_*` files may
  appear.
- The AUR ssh key is wired at `~/.ssh/aur` (`Host aur.archlinux.org`,
  `User aur`) and **has a passphrase**: a non-interactive `git push` to
  the AUR fails with `Permission denied` — **the user must be present**
  to supply it.
- GPG is **cert-only** (cannot sign commits), so AUR commits are
  **unsigned**.
- AUR publishing uses branch **`master`** (not `main`): `git push -u
  origin master`.

## AUR publish — current state

- **Release source package** — `packaging/scx-manager/`: PKGBUILD
  `pkgrel=2`, source `scx-manager-1.15.12.tar.gz`, `.SRCINFO`; the
  package builds from source at install time.
- **`-git` template** — `packaging/scx-manager-git/`: tracks the public
  fork (`git+<GIT_REPO_URL>#branch=main`, `pkgver()` resolves the short
  hash of `refs/heads/main`). `PKGURL`/`url` are still the
  `<GIT_REPO_URL>` placeholder — fill in with the public GitHub clone
  URL before publishing.
- **Deprecated `-bin`** — `aur-publish/` is gitignored staging from a
  pre-built variant that was built and then dropped. **Do not publish
  it.**

**Model:** for build-from-git, the **public GitHub fork IS the source**
of the package (and of the GPL). Publishing therefore means: create the
fork → push the de-branded code → point the package at the fork → push
the package files to the AUR.

## Next-cycle tasks

1. **GitHub repo:** user creates an empty **public** GitHub repo (e.g.
   `scx-manager`) and authenticates.
2. **Push the fork:** set a git remote to that repo and push the
   de-branded `main` branch (keep it as `main` so the `-git`
   PKGBUILD's `#branch=main` and `pkgver()` resolve; if the fork
   defaults to `master`, update the PKGBUILD branch reference instead).
   User present for the passphrase.
3. **Verify the fork is clean:** no `AUR KIT/`, no `aur-publish/`,
   correct layout, and `README.md` / `BLUEPRINT.md` / `HANDOVER.md`
   present; no secrets in the pushed diff.
4. **Point packages at the fork:** set the `scx-manager-git` PKGBUILD
   `PKGURL` to the GitHub clone URL, and make the release package's
   source tarball match the published tree (or use the `-git` package
   instead).
5. **Package hygiene:** generate `.SRCINFO` (makepkg), run `namcap`, and
   build in a clean chroot (`extra-x86_64-build`) if available.
6. **Create the AUR package:** `git clone
   git@aur.archlinux.org:scx-manager(-git).git`, copy in
   `PKGBUILD` / `.SRCINFO` / source (the release tarball for the release
   package), commit, and `git push -u origin master` (branch **master**,
   not main; user supplies the passphrase).
7. **Confirm:** the AUR page shows the package with the correct version
   and dependencies.

## Kick-off prompt for the next dev cycle

Paste this into a fresh context to resume:

```text
You are continuing the Arch port of scx-manager (repo scx-manager,
branch main, tree clean). The de-brand is COMPLETE and verified, and
all work is on main. First read BLUEPRINT.md, README.md, and
HANDOVER.md in the repo root. The next task is to publish: (a) push
the de-branded repo to a new public GitHub fork — the user will handle
authentication and the key passphrase; do NOT read, echo, or handle
any keys — then (b) create and publish the AUR package
(scx-manager or scx-manager-git) per HANDOVER.md "Next-cycle tasks".
NEVER upload the AUR KIT or any key; before every push run git status
and git check-ignore and confirm no secret appears in the staged
diff. AUR pushes use branch master (not main) and the user must be
present for the ssh passphrase; GPG is cert-only so commits are
unsigned.
```
