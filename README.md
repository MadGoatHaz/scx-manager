# SCX Scheduler Manager (`scx-manager`)

A distro-agnostic Qt GUI to manage sched-ext (SCX) schedulers via the
`scx_loader` daemon.

## What it does

SCX Scheduler Manager lists the sched-ext schedulers that the running
`scx_loader` daemon reports as supported (queried over D-Bus,
`org.scx.Loader`) and lets you activate or deactivate them from a simple
Qt6 interface. Scheduler loading is elevated through `pkexec`, so the
app itself runs as your normal user. If the daemon is not present, the
app degrades gracefully instead of failing.

## Features

- Lists all schedulers the running `scx_loader` supports
- Activate / deactivate schedulers with polkit elevation (`pkexec`)
- Graceful handling when `scx_loader` is absent ("Cannot get information
  from scx_loader!")
- Localized UI strings (Qt Linguist `.ts` files under `lang/`)
- Neutral desktop integration: `scx-manager.desktop` + hicolor
  `scx-manager.svg` icon
- Warning-clean from-scratch Release build

## Requirements

- A **sched-ext capable kernel** — `CONFIG_SCHED_EXT` enabled
- **scx-tools** — provides the `scx_loader` daemon (`org.scx.Loader`
  D-Bus service)
- **scx-scheds** — the scheduler binaries
- **Qt6** — to build (plus CMake and a C++23 compiler; tested with
  GCC 14.1.1 and Clang 18)

## Available schedulers

scx-manager lists the schedulers that the running **scx_loader** daemon reports as supported (queried over D-Bus, `org.scx.Loader`). You can see the same list any time with:

    scxctl list

The set depends on which schedulers are installed (the **scx-scheds** package) and which the loaded scx_loader version supports. On this system the manager currently offers:

| Scheduler   | What it is (self-reported) |
|-------------|----------------------------|
| `scx_beerland`    | Scheduler designed to prioritize locality and scalability. |
| `scx_bpfland`     | A vruntime-based sched-ext scheduler that prioritizes interactive workloads. |
| `scx_cake`        | A sched-ext scheduler applying CAKE bufferbloat concepts. |
| `scx_cosmos`      | Lightweight scheduler optimized for preserving task-to-CPU locality. |
| `scx_flash`       | A scheduler that focuses on ensuring fairness and performance predictability. |
| `scx_flow`        | *(no self-description in this build — see `scxctl list`)* |
| `scx_forge`       | *(no self-description in this build — see `scxctl list`)* |
| `scx_lavd`        | Latency-criticality Aware Virtual Deadline (LAVD) scheduler. |
| `scx_p2dq`        | A "pick 2" dumb-queue load-balancing scheduler. |
| `scx_pandemonium` | An adaptive Linux scheduler. |
| `scx_rustland`    | User-space scheduler written in Rust. |
| `scx_rusty`       | A multi-domain BPF / userspace hybrid scheduler. |
| `scx_tickless`    | *(no self-description in this build — see `scxctl list`)* |

Notes:
- `scx_chaos` and `scx_layered` are also installed via **scx-scheds** but are not in the current scx_loader supported set, so they do not appear in the manager.
- More schedulers are available across the [sched-ext](https://github.com/sched-ext/scx) ecosystem; install the **scx-scheds** package for the common set.

## Building from source

Configure and build with CMake (Unix Makefiles generator by default;
Ninja is supported as an alternative):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # add -G Ninja if preferred
cmake --build build
```

`configure.sh` wraps the configure step. The `fmt` dependency is
fetched via CPM, built header-only, and configured with
`FMT_SYSTEM_HEADERS=ON`, so it adds no build warnings. The Rust
component (`scx-rustlib`) is built by Corrosion during the CMake build;
a Rust toolchain (`rust` package, provides `cargo`) is required.

## Installing (Arch / AUR)

The AUR package builds from source at install time. The release package
(`scx-manager`) ships the release source tarball; a `scx-manager-git`
package will point at the public source repository. Runtime
dependencies: `qt6-base`, `polkit`, `scx-tools`. Build
dependencies: `cmake`, `git`, `qt6-base`, `rust`.

## License & provenance

SCX Scheduler Manager (`scx-manager`) is an independent,
distribution-agnostic downstream rebrand and port of the upstream
[CachyOS scx-manager](https://github.com/CachyOS/scx-manager) project.

**License:** GPL-3.0-or-later; the full license text is in `LICENSE`.

**History & attribution:** this repository preserves the complete
upstream git history — every original commit and author attribution
from CachyOS/scx-manager is intact, and the upstream copyright and
provenance headers inside the source files are deliberately retained
(not stripped during de-branding); they are attribution, not branding.

**Relationship:** this project is community-maintained and is **not**
affiliated with, sponsored by, or endorsed by CachyOS. Upstream
retains copyright over its original contributions; all modifications in
this repository are released under the same GPL-3.0-or-later license.

**Credits:** original work by Vladislav Nepogodin and the upstream
contributors (see `git log` for full attribution).

## Layout

The repository is organized for a GitHub fork of the upstream project:

- `src/` — C++23 / Qt6 application sources
- `scx-rustlib/` — Rust crate (cxx bridge client, D-Bus via zbus)
- `cmake/` — CMake helpers (CPM, warning flags, sanitizers, install
  config)
- `include/` — public headers
- `lang/` — Qt translation files (`.ts`)
- `packaging/` — AUR packages (`scx-manager/`, `scx-manager-git/`)
- root — `CMakeLists.txt`, `configure.sh`, `scx-manager.desktop`,
  `scx-manager.svg`, `LICENSE`, `README.md`
