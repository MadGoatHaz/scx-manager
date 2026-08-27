# SCX Scheduler Manager — Project Blueprint

## 1. Overview

**SCX Scheduler Manager** (`scx-manager`) is a distro-agnostic Qt GUI for
managing sched-ext (SCX) schedulers via the `scx_loader` daemon. This
repository is a neutral rebrand of the upstream **CachyOS/scx-manager**
project (upstream base commit `af37c3e7bbffa6b259c3d0aec88da33c6e0062b0`,
version **1.15.12**).

The rebrand goal: strip all CachyOS branding from the user-visible
identity, desktop integration, and documentation, and replace it with
neutral/Arch identity strings (display "SCX Scheduler Manager",
organization "Arch Linux", org domain `archlinux.org`, reverse-DNS
`org.archlinux.scx-manager`). The upstream license, **GPL-3.0-or-later**,
is **retained**, and the upstream copyright/provenance attribution
remains in the source.

## 2. Architecture

The application is a C++23 / Qt6 (Widgets, LinguistTools) program that
delegates scheduler operations to a Rust crate over a cxx bridge. There
are **no in-repo polkit files**: elevation is `pkexec` invoked from the
Rust layer, and the D-Bus service `org.scx.Loader` is provided by the
external **scx-tools** package (the `scx_loader` daemon).

```
 scx-manager (C++23 / Qt6)
 ┌───────────────────────────────────────────────────────────┐
 │  schedext-window (.ui) ──► scxctl-ui (Qt UI shared lib)   │
 │                 │                                         │
 │                 ▼ (cxx bridge)                            │
 │  scx-lib-cxxbridge ──► scx-rustlib (Rust crate,           │
 │                         built by Corrosion v0.6.1)        │
 │                 │                 │                       │
 │                 ▼                 ▼                       │
 │           pkexec (elevation)   zbus ──► D-Bus             │
 │                                 org.scx.Loader            │
 │                                 (scx-tools daemon)        │
 │                                     │                     │
 │                                     ▼                     │
 │                            SCX scheduler binaries         │
 │                            (scx-scheds)                   │
 └───────────────────────────────────────────────────────────┘
          Requires a sched-ext kernel (CONFIG_SCHED_EXT)
```

Two libraries are produced: **`scxctl-ui`** (the Qt UI shared library,
with installed CMake config) and **`scx-lib-cxxbridge`** (the cxx bridge).
Scheduler discovery: the Rust layer asks the daemon over D-Bus
(`LoaderClientProxy::supported_schedulers()`), and the UI lists whatever
the running loader reports.

## 3. Build system

- **CMake** with the Unix **Makefiles** generator (default); **Ninja** is
  a supported alternative (`-G Ninja`).
- The **`scx-rustlib`** crate is built by **Corrosion v0.6.1**, itself
  fetched at configure time via **CPM** (`cmake/CPM.cmake`).
- **fmt 12.1.0** is CPM-fetched, **header-only**, and configured with
  **`FMT_SYSTEM_HEADERS=ON`** so it adds no build warnings.
- **Cargo dependencies used by the crate:** `cxx` 1.0.196, `zbus` 5.16,
  `tokio`, `scx_loader` 1.1.2, `toml`, `anyhow`.
- `configure.sh` is a convenience wrapper around the CMake configure
  step; the release source package (section 6) invokes `cmake` directly.

## 4. De-brand mapping

| Upstream (CachyOS) | This port (neutral/Arch) |
|---|---|
| CachyOS display name | **"SCX Scheduler Manager"** (window title) |
| App name `scx-manager` | `scx-manager` (kept) |
| CachyOS organization | **"Arch Linux"** |
| CachyOS org domain | **`archlinux.org`** |
| CachyOS reverse-DNS id | **`org.archlinux.scx-manager`** |
| Branded icon | **`scx-manager.svg`** (hicolor/scalable) |
| CachyOS desktop entry | **`scx-manager.desktop`** |

**Intentionally retained** (upstream attribution, not UI branding — do not
treat these as defects):

- The GPL **LICENSE** file (license retained; upstream copyright retained).
- The **7 source-file provenance headers** reading *"This file is part of
  CachyOS kernel manager."*
- The **Rust copyright email in `scx-rustlib/src/utils.rs`**.

## 5. Runtime & interop

- **Kernel:** a sched-ext capable kernel is required — `CONFIG_SCHED_EXT`.
- **scx-tools:** provides the `scx_loader` daemon and the
  `org.scx.Loader` D-Bus service.
- **scx-scheds:** provides the scheduler binaries.
- **Discovery:** the app lists exactly what the running `scx_loader`
  reports as supported (`LoaderClientProxy::supported_schedulers()`);
  when the daemon is absent it degrades gracefully (shows "Cannot get
  information from scx_loader!").

On the reference test system `scx-scheds` installed **15** scheduler
binaries, but `scx_loader` reported **13** as supported:

- **beerland** — prioritizes locality and scalability
- **bpfland** — vruntime-based, interactive workloads
- **cake** — CAKE bufferbloat concepts
- **cosmos** — lightweight, preserves task-to-CPU locality
- **flash** — fairness and performance predictability
- **flow** — (no self-description in this build)
- **forge** — (no self-description in this build)
- **lavd** — Latency-criticality Aware Virtual Deadline
- **pandemonium** — adaptive Linux scheduler
- **p2dq** — "pick 2" dumb-queue load balancing
- **rustland** — user-space scheduler written in Rust
- **rusty** — multi-domain BPF / userspace hybrid
- **tickless** — (no self-description in this build)

`scx_chaos` and `scx_layered` are installed but not in the loader's
supported set, so they do not appear.

## 6. Packaging (AUR)

Two package directories ship under `packaging/`:

| Directory | Model | Notes |
|---|---|---|
| `packaging/scx-manager/` | Release **source** package | PKGBUILD `pkgrel=2`; source `scx-manager-1.15.12.tar.gz` + `.SRCINFO`; builds from source at install time (CMake Release, Makefiles generator) |
| `packaging/scx-manager-git/` | **`-git`** template | Tracks the public fork (`git+<GIT_REPO_URL>#branch=main`, `pkgver()` resolves the short hash); `PKGURL` is still the `<GIT_REPO_URL>` placeholder — fill in with the public GitHub URL before publishing |

The pre-built **`-bin`** variant (staged under `aur-publish/`) was built
and then **deprecated** — it is gitignored and must **not** be published.
The decision is a **build-from-git source model**: the public source
repository is the package source, which also satisfies the GPL.

Dependencies (both packages):

- `depends=(qt6-base polkit scx-tools)` — the Qt6 UI toolkit, polkit-based
  elevation, and the `scx_loader` daemon.
- `makedepends=(cmake git qt6-base rust)` — the CMake build, git for the
  CPM/Corrosion fetches, and the Rust toolchain (`cargo` is provided by
  the `rust` package).
- `optdepends` lists `scx-scheds` (scheduler implementations) and a
  `sched-ext-kernel` marker documenting the required kernel feature.

## 7. Verification performed

- From-scratch **Release build: 0 compiler warnings** (scoped pure pragma
  + `FMT_SYSTEM_HEADERS=ON`).
- `ldd` on the built binary: **0 missing** libraries.
- **Install resolves** with the expected layout under `usr/`: `bin/`,
  desktop file, `scx-manager.svg` (hicolor/scalable), libraries, headers,
  and CMake config.
- **Graceful degradation** when `scx_loader` is absent: the UI shows
  "Cannot get information from scx_loader!" instead of failing.
- Packaged `.pkg`: **0 placeholders**, correct file layout.
- Translation `.ts` files regenerated via `lupdate -no-obsolete`; the
  "Cancel" → "Close" button fix was corrected to a clean `Close` entry.
- **De-brand sweep: 0 shipped defects** (no remaining CachyOS branding in
  user-visible strings, desktop file, icon, or README/`configure.sh`).

## 8. Decisions log

| Decision | Rationale |
|---|---|
| Retain GPL-3.0-or-later + upstream copyright | Legal continuity for the derivative; attribution kept |
| Neutral "SCX Scheduler Manager" / Arch Linux identity | Distro-agnostic goal; no vendor branding in UI surfaces |
| Build-from-git source model (drop the `-bin`) | GPL requires source availability; the public repo is the source; simpler to maintain |
| Corrosion v0.6.1 + fmt via CPM, header-only, `FMT_SYSTEM_HEADERS=ON` | Reproducible build, zero added warnings |
| Elevation via `pkexec` from Rust; no in-repo polkit files | Single elevation path; matches upstream design |
| `AUR KIT` gitignored, never uploaded | Holds private keys; must never reach git/GitHub/AUR |
| `-git` package tracks the fork's `main` branch | `pkgver()` resolves `refs/heads/main` of the public repo |

## 9. Roadmap / next cycle

- Publish the de-branded repo as a **public GitHub fork** (the fork is
  the GPL source and the source of the `-git` package).
- Create and publish the **AUR package** (`scx-manager` release source
  package, or `scx-manager-git` once `PKGURL` is filled in).

See **`HANDOVER.md`** for the concrete next-cycle steps, the security
checklist, and the kick-off prompt.
