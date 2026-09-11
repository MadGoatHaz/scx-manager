# scx-manager

Manage sched-ext (SCX) schedulers from a clean Qt6 desktop app.

[![Build](https://github.com/MadGoatHaz/scx-manager/actions/workflows/build.yml/badge.svg)](https://github.com/MadGoatHaz/scx-manager/actions/workflows/build.yml)
[![Checks](https://github.com/MadGoatHaz/scx-manager/actions/workflows/checks.yml/badge.svg)](https://github.com/MadGoatHaz/scx-manager/actions/workflows/checks.yml)
[![AUR version](https://img.shields.io/aur/version/scx-manager.svg)](https://aur.archlinux.org/packages/scx-manager)
[![AUR version](https://img.shields.io/badge/AUR-scx--manager--bin--1.15.12--1-blue.svg)](https://aur.archlinux.org/packages/scx-manager-bin)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/License-GPL--3.0--or--later-blue)](LICENSE)

scx-manager lists the sched-ext schedulers that the running `scx_loader` daemon reports as supported (queried over D-Bus, `org.scx.Loader`, via `scx-tools`) and lets you activate or deactivate them with one of five tuning profiles. Scheduler loading is elevated through `pkexec`, so the app itself always runs as your normal user. If the daemon is not present, the app degrades gracefully — a critical dialog, and it stays alive.

📖 **[Full User Guide](docs/USER_GUIDE.md)** — per-scheduler deep dives, profiles, requirements, and troubleshooting.

## Features

- **Live scheduler detection** — the list is whatever the running `scx_loader` daemon reports as supported (queried over D-Bus), so it always matches your installed tooling; you can see the same set any time with `scxctl list`.
- **Data-driven info card** — each scheduler's title, tagline, summary, supported hardware, and typical workloads come from the embedded `resources/scheduler-metadata.json` catalog.
- **Five tuning profiles** — Auto, Gaming, Powersave, Lowlatency, and Server, each applying per-scheduler tuning on top of the selected scheduler (see [Profiles](#profiles)).
- **Service & config management** — enable/disable the `scx_loader` daemon and manage its configuration (`/etc/scx_loader.toml`) through `pkexec` elevation.
- **Graceful degradation** — if the `scx_loader` daemon is absent, the app shows a critical dialog and stays alive instead of failing.
- **Localized UI** — 11 Qt Linguist `.ts` catalogs under `lang/`.
- **Desktop integration** — `scx-manager.desktop` entry + hicolor scalable SVG icon.
- **Warning-clean build** — from-scratch Release build with zero compiler warnings.

## Supported schedulers

scx-manager shows the schedulers that the running **`scx_loader`** daemon reports as supported (queried over D-Bus, `org.scx.Loader`); you can see the same list any time with:

    scxctl list

The visible set is what **`scx-scheds`** has installed ∩ what the loaded `scx_loader` version supports. The embedded metadata catalog documents all 13:

| Scheduler | Name | What it does | Typical workloads |
|---|---|---|---|
| `scx_lavd` | LAVD (Latency-Aware Virtual Deadline) | Optimized for frame pacing and low input latency. | Gaming, Low Latency, General Desktop |
| `scx_bpfland` | BPFland (Interactive vruntime Scheduler) | Low-latency scheduling for smooth gaming and desktop responsiveness. | Gaming, Low Latency, Audio/Multimedia, Heavy Multitasking |
| `scx_cake` | CAKE (DRR++ 4-Tier CPU Scheduler) | Network-inspired deficit round robin for smooth gaming. | Gaming, Low Latency, Audio/Multimedia, General Desktop |
| `scx_cosmos` | Cosmos (Adaptive Locality & Saturation Scheduler) | Balanced throughput and responsiveness for desktop and server. | General Desktop, Server/Cloud, Throughput-Compilation |
| `scx_p2dq` | P2DQ (Pick-Two Multi-Queue Scheduler) | Datacenter-tested load balancer with multi-layer topology queues. | Server/Cloud, Throughput-Compilation, Gaming, Heavy Multitasking |
| `scx_tickless` | Tickless (HPC & Virtualization Scheduler) | Tickless compute cores for jitter-free server execution. | HPC/Virtualization, Server/Cloud, Throughput-Compilation |
| `scx_beerland` | Beerland (Locality-Aware Non-blocking Dispatcher) | Scalable, cache-local scheduling for high-core systems. | Throughput-Compilation, Server/Cloud, Heavy Multitasking, General Desktop |
| `scx_flash` | Flash (Earliest Deadline First Fairness) | Predictable, fair-share latency scheduling for audio and multimedia. | Audio/Multimedia, Low Latency, Server/Cloud |
| `scx_flow` | Flow (Budget-Driven Deterministic Scheduler) | Zero-heuristic, bandwidth-allocated scheduling with real-time web telemetry. | Deterministic/Embedded, Low Latency, Audio/Multimedia |
| `scx_forge` | Forge (AI-Agent Dynamic Optimizer) | Adaptive scheduling tuned dynamically via user-space telemetry agents. | Research/Experimental, Server/Cloud |
| `scx_pandemonium` | Pandemonium (Behavioral Interconnect Scheduler) | Advanced interconnect-aware scheduler with CoDel latency protection. | Gaming, Heavy Multitasking, Throughput-Compilation, Audio/Multimedia |
| `scx_rustland` | Rustland (User-Space Reference Scheduler) | Educational user-space scheduling engine built in Rust. | Research/Experimental, General Desktop |
| `scx_rusty` | Rusty (Multi-Domain LLC Load Balancer) | Hybrid round-robin cache domain scheduling with user-space balancing. | Server/Cloud, Throughput-Compilation |

More schedulers are available across the [sched-ext](https://github.com/sched-ext/scx) ecosystem.

## Profiles

Five profiles select the tuning applied to whichever scheduler is active; each profile's per-scheduler behavior (slice lengths, CPU modes, feature flags) is documented in the [Full User Guide](docs/USER_GUIDE.md).

| Profile | Behavior |
|---|---|
| Auto | Runs the scheduler with its default values. No special tuning is applied, making it the baseline the other profiles deviate from. |
| Gaming | Favors interactive responsiveness with shorter time-slices and performance-oriented CPU behavior, tuned for frame pacing and low input latency. |
| Powersave | Favors energy efficiency with a low-power CPU mode and longer slice quanta, reducing wakeups and inter-processor traffic. |
| Lowlatency | Favors minimal wakeup and queue latency with performance CPU mode and wakeup-preemption behavior. |
| Server | Favors sustained throughput with longer slices, strict CPU-affinity prioritization, and keep-running semantics. |

## How it works

The `scx_loader` daemon (from `scx-tools`) exposes `org.scx.Loader` on the system D-Bus. scx-manager's Rust bridge (Corrosion/cxx → zbus, `scx_loader` crate) queries the daemon for the supported schedulers and their configuration. Activating a scheduler + profile writes `/etc/scx_loader.toml` (via `pkexec /usr/bin/cp`) and runs `systemctl enable -f scx_loader` after your polkit approval; deactivating clears the scheduler entry the same way. The app itself never runs as root — all privileged operations go through the polkit dialog — and if the daemon is absent it shows a critical dialog and stays alive.

## Requirements

Runtime:

- A **sched-ext capable kernel** — `CONFIG_SCHED_EXT` enabled
- **`scx-tools`** (official `extra`) — provides the `scx_loader` daemon (the `org.scx.Loader` D-Bus service) and `scxctl`, and pulls in **`scx-scheds`** (the scheduler binaries)
- **`qt6-base`** and **`polkit`**

Building from source additionally needs:

- A **C++23 compiler** (GCC 14+ or Clang 18)
- **Qt6** (Widgets + LinguistTools)
- **CMake** ≥ 3.20
- **Rust** — for the Corrosion/cxx D-Bus bridge
- **git** — CPM fetches `fmt` and Corrosion at configure time

```sh
sudo pacman -S base-devel cmake qt6-tools rust
```

(`scx-tools` is not needed to build, only to run.)

## Installing

### From the AUR

`scx-manager` is published on the [Arch User Repository](https://aur.archlinux.org/packages/scx-manager):

```sh
# with any AUR helper (yay, paru, trizen, ...)
yay -S scx-manager
# or
paru -S scx-manager
```

### Packages

Three package flavors install the same application — they conflict with each other, so only one can be present. Pick the one that matches how you want it built:

| Entry | Build model | Install |
|-------|-------------|---------|
| [`scx-manager-bin`](https://aur.archlinux.org/packages/scx-manager-bin) | Precompiled x86_64 binary — zero build dependencies (no cmake, no cargo, no CPM fetches) | `yay -S scx-manager-bin` |
| [`scx-manager`](https://aur.archlinux.org/packages/scx-manager) | Source build in your AUR chroot (full build toolchain) | `yay -S scx-manager` |
| [`scx-manager-git`](https://github.com/MadGoatHaz/scx-manager/tree/main/packaging/scx-manager-git) | Rolling build of the `main` branch (AUR submission pending — build from the repo) | `makepkg -si` in `packaging/scx-manager-git/` |

- **Desktop users** → `scx-manager-bin`: one command, nothing compiled.
- **Reproducible or patched builds** → `scx-manager` (source): built from the release tarball in your AUR chroot with the full toolchain.
- **Latest unreleased fixes** → `scx-manager-git` (rolling `main`).
- **Switching** is one command in either direction: the flavors declare symmetric conflicts, so `pacman -S scx-manager` (or `pacman -S scx-manager-bin`) removes the other and installs the chosen package in a single transaction — no state migration needed.

### From source

```sh
git clone https://github.com/MadGoatHaz/scx-manager.git
cd scx-manager
```

The project is built with **CMake only** (there is no other build system). Configure and build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Run the built binary:

```sh
./build/scx-manager
```

Optionally install it for your user (no `sudo` required):

```sh
cmake --install build --prefix ~/.local
```

`configure.sh` is an alternative entry point: it wraps the CMake invocation and generates `build.sh`, which drives `cmake --build` (if you intend to install it globally, you might also want `--prefix=/usr`). The `fmt` dependency is fetched via CPM (header-only, `FMT_SYSTEM_HEADERS=ON`) together with Corrosion at configure time — hence the `git` requirement — and the Rust component (`scx-rustlib`) is built by Corrosion during the CMake build.

## Development

**Build from source** (see [From source](#from-source)):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/scx-manager
```

**Run the tests** — `ctest` drives the unit suite: `test_scheduler_metadata` (the metadata catalog contract) and `test_info_panel` (offscreen render of the info card across schedulers × profiles). The full suite is expected to pass with zero compiler warnings:

```sh
ctest --test-dir build
```

**Libraries:** Qt6 (GUI), fmt (string formatting, via CPM), and zbus / `scx_loader` (D-Bus, via Corrosion/cxx).

## License & provenance

scx-manager is licensed under the [GNU General Public License v3.0 or later](LICENSE) — the full license text is in `LICENSE`.

scx-manager is an independent, distribution-agnostic downstream rebrand and port of the upstream [CachyOS scx-manager](https://github.com/CachyOS/scx-manager) project.

**History & attribution** — this repository preserves the complete upstream git history: every original commit and author attribution from CachyOS/scx-manager is intact, and the upstream copyright and provenance headers inside the source files are deliberately retained (not stripped during de-branding). They are attribution, not branding.

**Relationship** — this project is community-maintained and is **not** affiliated with, sponsored by, or endorsed by CachyOS. Upstream retains copyright over its original contributions; all modifications in this repository are released under the same GPL-3.0-or-later license.

**Credits** — original work by Vladislav Nepogodin and the upstream contributors (see `git log` for full attribution).

## Layout

- `src/` — C++23 / Qt6 application sources
- `scx-rustlib/` — Rust crate (cxx bridge, D-Bus via zbus)
- `cmake/` — CMake helpers (CPM, warning flags, sanitizers, install config)
- `include/` — public headers
- `resources/` — embedded scheduler metadata catalog
- `lang/` — Qt translation files (`.ts`)
- `tests/` — unit tests + offscreen checks
- `packaging/` — AUR packages (`scx-manager/`, `scx-manager-git/`, `scx-manager-bin/`)
- `docs/` — user guide
- root — `CMakeLists.txt`, `configure.sh`, `scx-manager.desktop`, `scx-manager.svg`, `LICENSE`, `README.md`
