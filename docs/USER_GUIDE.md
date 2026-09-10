# scx-manager User Guide

This guide explains how scx-manager fits into the sched-ext (SCX) tool stack, what each of the 13 documented schedulers does, the five tuning profiles, the system requirements, the installation options, and how to troubleshoot common problems.

The project README ([../README.md](../README.md)) covers the feature overview, building from source, license & provenance, and repository layout.

Contents:

1. [Introduction](#introduction)
2. [How scx-manager works](#how-scx-manager-works)
3. [The schedulers](#the-schedulers)
4. [Profiles](#profiles)
5. [Requirements](#requirements)
6. [Installation options](#installation-options)
7. [Troubleshooting](#troubleshooting)
8. [The sched-ext ecosystem](#the-sched-ext-ecosystem)

## Introduction

### What sched-ext (SCX) is

sched-ext (SCX) is the Linux kernel's extensible scheduler class: a kernel-side framework (enabled by `CONFIG_SCHED_EXT`) in which the scheduling policy itself is written as a BPF (eBPF) program and loaded into the kernel. If no BPF scheduler is loaded, the kernel runs its default scheduler (CFS) — so unloading an SCX scheduler is always a safe, reversible operation.

On Arch Linux the userspace side of the stack is:

- **`scx-tools`** (official `extra`) — provides the **`scx_loader`** daemon (the service that loads, switches, and stops SCX schedulers and exposes the **`org.scx.Loader`** system D-Bus service) and the **`scxctl`** command-line interface (e.g. `scxctl list`, `scxctl stop`).
- **`scx-scheds`** (pulled in by `scx-tools`) — the **`scx-*`** scheduler binaries, one per scheduler (e.g. `scx_lavd`, `scx_bpfland`, `scx_p2dq`), each carrying the scheduler's BPF program.
- **scx-manager** (this application) — the GUI front end for that stack. It talks to the running `scx_loader` daemon over D-Bus, lists the schedulers the daemon supports, lets you activate one with a tuning profile (or deactivate it), and keeps the daemon's configuration in sync — all without running as root.

## How scx-manager works

### Architecture

scx-manager is a Qt6 / C++23 application. A Rust bridge (`scx-rustlib`, statically linked into the binary via Corrosion/cxx) talks to the daemon over the **system D-Bus** using zbus and the `scx_loader` crate; the daemon's service name is **`org.scx.Loader`**. From it the app queries:

- the **supported schedulers** — exactly what populates the scheduler dropdown,
- the **currently running scheduler**, and
- the **current scheduler mode**.

The scheduler **info card** (title, tagline, technical summary, supported-hardware chips, typical-workload chips) is data-driven from the embedded catalog `resources/scheduler-metadata.json` — no network access is needed. For a running scheduler that is not in the catalog, the card shows a fallback notice instead. The "currently running scheduler" indicator is refreshed once per second by reading `/sys/kernel/sched_ext/state` and `/sys/kernel/sched_ext/root/ops` directly from the kernel, so it keeps working even when the daemon is absent.

### Activating a scheduler (Apply)

1. If the legacy `scx.service` is enabled or active, it is stopped first so the two loader mechanisms do not conflict.
2. The daemon is asked, over `org.scx.Loader`, to switch to the selected scheduler + profile: if the resolved arguments equal the profile's defaults, the mode-based switch is used; otherwise the argument-based switch is used (which also covers any extra flags typed into the flags field). The chosen BPF scheduler is loaded into the kernel immediately.
3. If the `scx_loader` service is not enabled yet, it is enabled with `systemctl enable -f scx_loader`, so the selected scheduler starts automatically at boot.
4. The updated configuration (default scheduler, mode, per-mode arguments) is written to `/tmp/scx_loader.toml` and copied to **`/etc/scx_loader.toml`** with `pkexec /usr/bin/cp` — the only privileged step. The polkit approval dialog is presented for that copy; the application itself never runs as root.

### Deactivating (Disable)

The default scheduler entry in the configuration is cleared, the daemon is asked to stop the running scheduler over D-Bus (the kernel falls back to the default CFS scheduler), the configuration is staged in `/tmp/scx_loader.toml`, and `pkexec` copies it back to `/etc/scx_loader.toml`.

### Graceful degradation

If the `scx_loader` daemon is not present (not installed, or not running), the app shows a critical dialog:

> Cannot get information from scx_loader!
> Is it working?
> This is needed for the app to work properly

It then hides the scheduler, profile, and flags controls and **stays alive**: the current-scheduler indicator keeps updating from the kernel's `sched_ext` sysfs on its timer. Installing or restarting `scx-tools` (see [Troubleshooting](#troubleshooting)) brings the full UI back on the next launch.

## The schedulers

The dropdown lists exactly what the running `scx_loader` daemon reports as supported; you can see the same set any time with:

```sh
scxctl list
```

The visible set is what **`scx-scheds`** has installed ∩ what the loaded `scx_loader` version supports. The embedded catalog documents all 13 of them. Descriptions, hardware, and workloads below come from the catalog (`resources/scheduler-metadata.json`).

### `scx_lavd` — LAVD (Latency-Aware Virtual Deadline)

*Optimized for frame pacing and low input latency.*

Measures each task's latency criticality dynamically and modulates its virtual deadline and time slice, routing tasks across scheduling domains partitioned per LLC, per core type (Intel P/E, ARM big.LITTLE) and per NUMA node. Uses scx_bpf_now() to cut hardware TSC reads by ~76%. Author: Changwoo Min / Igalia.

- **Hardware:** AMD X3D, Intel P-E hybrid, ARM/embedded
- **Workloads:** Gaming, Low Latency, General Desktop

### `scx_bpfland` — BPFland (Interactive vruntime Scheduler)

*Low-latency scheduling for smooth gaming and desktop responsiveness.*

Derived from scx_rustland but runs the engine entirely in in-kernel eBPF (a thin Rust harness handles parameters and telemetry only). Tasks are sorted in a dual-tier weighted-vruntime topology: threads whose voluntary context-switch rate crosses a threshold are classified interactive, get priority queues, and receive an unused-time-slice carryover budget for bursty frame pacing.

- **Hardware:** Intel P-E hybrid, AMD multi-CCD, standard multi-core
- **Workloads:** Gaming, Low Latency, Audio/Multimedia, Heavy Multitasking

### `scx_cake` — CAKE (DRR++ 4-Tier CPU Scheduler)

*Network-inspired deficit round robin for smooth gaming.*

Adapts the network CAKE queue's Deficit Round Robin++ (DRR++) to CPU threads via a 4-tier classification driven by an EWMA of runtime duration (Critical <100us, Interactive <2ms, Frame <8ms, Bulk >=8ms) with 10% deadband hysteresis. Avoids global atomics using per-CPU BSS with MESI cache-line isolation, kernel-delegated idle selection, and per-LLC DSQ sharding. Author: RitzDaCat.

- **Hardware:** AMD X3D, AMD multi-CCD, Intel P-E hybrid
- **Workloads:** Gaming, Low Latency, Audio/Multimedia, General Desktop

### `scx_cosmos` — Cosmos (Adaptive Locality & Saturation Scheduler)

*Balanced throughput and responsiveness for desktop and server.*

Lightweight and adaptive: under normal load tasks are pinned to local per-CPU DSQs (maximizing cache hits, avoiding shared-queue spinlocks); on detected saturation it shifts to a global/per-NUMA earliest-deadline-first shared queue for work conservation. Batches/defers wakeups via a timer to prevent enqueue interrupt storms, using micro time-slices. Author: Andrea Righi.

- **Hardware:** NUMA/multi-socket, AMD multi-CCD, standard multi-core
- **Workloads:** General Desktop, Server/Cloud, Throughput-Compilation

### `scx_p2dq` — P2DQ (Pick-Two Multi-Queue Scheduler)

*Datacenter-tested load balancer with multi-layer topology queues.*

General-purpose 'Power-of-Two Choices' (Pick-2) load balancer implemented entirely in BPF with BPF Arenas for dynamic task state. Each LLC domain keeps three queues (main, interactive for low-utilization tasks, migration); a CPU checks local-LLC queues in order and, when empty, samples two LLCs and steals from the heavier migration queue. Supports hybrid P/E routing via a sched_core_priority min-heap; does NOT support CPU hotplugging. Author: Daniel Hodges (Meta).

- **Hardware:** AMD multi-CCD, Intel P-E hybrid, ARM/embedded, NUMA/multi-socket
- **Workloads:** Server/Cloud, Throughput-Compilation, Gaming, Heavy Multitasking

### `scx_tickless` — Tickless (HPC & Virtualization Scheduler)

*Tickless compute cores for jitter-free server execution.*

Server-oriented: a small housekeeping pool of primary CPUs (default CPU 0, --primary-domain) handles all scheduling events, timer ticks and queue routing, while worker CPUs run tickless with long/infinite slices (SCX_SLICE_INF) and receive preemption IPIs only on contention. Fully suppressing ticks requires the host kernel booted with nohz_full. Unsuitable for interactive desktop/gaming. Author: Andrea Righi.

- **Hardware:** NUMA/multi-socket, standard multi-core
- **Workloads:** HPC/Virtualization, Server/Cloud, Throughput-Compilation

### `scx_beerland` — Beerland (Locality-Aware Non-blocking Dispatcher)

*Scalable, cache-local scheduling for high-core systems.*

Prioritizes cache residency and scalable dispatch: per-CPU DSQs ordered by virtual deadline, with tasks pinned to their local core under normal load (migration only at wakeup via select_cpu). Under saturation it switches to distributed work-stealing - idle cores steal the earliest-deadline task from remote per-CPU DSQs, balancing work conservation with lock-free local execution. Author: Andrea Righi.

- **Hardware:** AMD multi-CCD, NUMA/multi-socket, standard multi-core
- **Workloads:** Throughput-Compilation, Server/Cloud, Heavy Multitasking, General Desktop

### `scx_flash` — Flash (Earliest Deadline First Fairness)

*Predictable, fair-share latency scheduling for audio and multimedia.*

EDF scheduler focused on strict fair-share and predictable latency under overcommit: each thread carries a latency weight that increments when it releases its slice early (advancing its virtual deadline) and decays when it exhausts the full slice, so bursty interactive work preempts compute-bound bulk work. Author: Andrea Righi.

- **Hardware:** standard multi-core
- **Workloads:** Audio/Multimedia, Low Latency, Server/Cloud

### `scx_flow` — Flow (Budget-Driven Deterministic Scheduler)

*Zero-heuristic, bandwidth-allocated scheduling with real-time web telemetry.*

Budget-driven with no heuristic classification: sleeping tasks accumulate execution budget, running tasks consume it monotonically, and each thread gets an equal share of core bandwidth (fair sharing, no priority inversion, no starvation). Wakeups bypass intermediate layers via immediate SCX_DSQ_LOCAL_ON dispatch; re-enqueues enter a per-CPU vtime-ordered 'Waiting Room' with slice clamped to [50us, 2ms]. Ships a live web dashboard. Author: Galih Tama.

- **Hardware:** ARM/embedded, standard multi-core
- **Workloads:** Deterministic/Embedded, Low Latency, Audio/Multimedia

### `scx_forge` — Forge (AI-Agent Dynamic Optimizer)

*Adaptive scheduling tuned dynamically via user-space telemetry agents.*

Experimental agent-driven scheduler: an efficient in-kernel BPF dispatcher provides the low-overhead data path while a companion user-space process (scx_forge_agent) monitors hardware telemetry (cache misses, IPC, branch mispredictions) and adjusts dispatch parameters in a closed loop. Scales across enterprise NUMA and GPU-adjacent hosts; agent feedback loops can lag on phase changes. Author: Andrea Righi.

- **Hardware:** NUMA/multi-socket, standard multi-core
- **Workloads:** Research/Experimental, Server/Cloud

### `scx_pandemonium` — Pandemonium (Behavioral Interconnect Scheduler)

*Advanced interconnect-aware scheduler with CoDel latency protection.*

Behavioral adaptive scheduler with 3-tier classification (idle placement via kick preemption, cache-hot wakeup placement, batch overflow isolation), L2/L3 affinity tracking, CoDel sojourn enforcement, and automated process learning, built on a hybrid C23/eBPF engine with an async Rust control loop. Cross-domain work-stealing is gated by an effective-resistance metric (Reff) and a precomputed graph-conductance cut price (Phi). Author: William Clingan.

- **Hardware:** AMD multi-CCD, AMD X3D, Intel P-E hybrid
- **Workloads:** Gaming, Heavy Multitasking, Throughput-Compilation, Audio/Multimedia

### `scx_rustland` — Rustland (User-Space Reference Scheduler)

*Educational user-space scheduling engine built in Rust.*

Hybrid reference scheduler: a user-space Rust daemon makes the primary scheduling decisions - tracking voluntary context switches, computing dynamic priority weights, and ordering tasks by weighted vruntime - over a minimal BPF shim (scx_rustland_core) that relays lifecycle events via ring buffers and shared maps. Substantial kernel-user IPC overhead makes it unsuitable for low-latency audio or high-throughput I/O; best for prototyping and study. Author: Andrea Righi.

- **Hardware:** standard multi-core
- **Workloads:** Research/Experimental, General Desktop

### `scx_rusty` — Rusty (Multi-Domain LLC Load Balancer)

*Hybrid round-robin cache domain scheduling with user-space balancing.*

Multi-domain hybrid: the execution path runs in kernel BPF doing fast round-robin dispatch within LLC-mapped domains, while an async user-space Rust daemon computes per-domain load factors and writes cross-domain migration directives into the lb_data kernel map. Migration directives lag sudden workload transitions, and high task-weight disparities can cause infeasible-weight issues. Author: Tejun Heo.

- **Hardware:** AMD multi-CCD, NUMA/multi-socket
- **Workloads:** Server/Cloud, Throughput-Compilation

> Note: **`scx_chaos`** and **`scx_layered`** are also installed via the **`scx-scheds`** package but are not in the current `scx_loader` supported set, so they do not appear in scx-manager (nor in `scxctl list`). The 13 entries above are the complete documented set.

## Profiles

Five profiles select the tuning applied to whichever scheduler is active: **Auto**, **Gaming**, **Powersave**, **Lowlatency**, and **Server**.

The profile dropdown is offered for the six schedulers that expose per-profile tuning presets in the UI — `scx_bpfland`, `scx_cake`, `scx_cosmos`, `scx_lavd`, `scx_p2dq`, `scx_tickless` — and is hidden for the others. Selecting a scheduler + profile fills the flags field with the arguments the daemon resolves for that combination; you may edit the field to add extra flags before pressing Apply. The card's "Active Profile" section shows the per-scheduler variant from the catalog when one is defined for the selected scheduler, and the generic profile description otherwise.

### Auto

Runs the scheduler with its default values. No special tuning is applied, making it the baseline the other profiles deviate from.

Per-scheduler behavior:

| Scheduler | Behavior |
|---|---|
| `scx_lavd` | Runs LAVD in autopilot mode, dynamically balancing latency criticality without manual tuning. |
| `scx_beerland` | Uses Beerland's balanced default, combining local pinning with distributed work-stealing under saturation. |
| `scx_pandemonium` | Runs Pandemonium in its default adaptive mode, letting the interconnect-aware heuristics self-tune. |

Schedulers without a dedicated Auto variant simply run their defaults.

### Gaming

Favors interactive responsiveness with shorter time-slices and performance-oriented CPU behavior, tuned for frame pacing and low input latency.

Per-scheduler behavior:

| Scheduler | Behavior |
|---|---|
| `scx_lavd` | Enables LAVD's performance mode with a pinned 500 us slice for steady frame pacing. |
| `scx_bpfland` | Runs BPFland in performance mode with wakeup preemption for responsive interactive threads. |
| `scx_cake` | Selects CAKE's gaming profile for interactive frame delivery. |
| `scx_cosmos` | Raises Cosmos' slice to 700 us for smoother interactive performance. |
| `scx_p2dq` | Enables P2DQ task slicing and performance scheduling mode for low-latency dispatch. |
| `scx_tickless` | Applies Tickless' non-standard gaming frequency and slice settings. Not recommended for interactive desktop use. |

### Powersave

Favors energy efficiency with a low-power CPU mode and longer slice quanta, reducing wakeups and inter-processor traffic.

Per-scheduler behavior:

| Scheduler | Behavior |
|---|---|
| `scx_lavd` | Switches LAVD to powersave mode with a pinned 500 us slice. |
| `scx_bpfland` | Runs BPFland in powersave mode with a 20000 us slice and lowered interactivity thresholds. |
| `scx_cake` | Selects CAKE's battery profile for energy-efficient dispatch. |
| `scx_cosmos` | Runs Cosmos in powersave CPU mode. |
| `scx_p2dq` | Switches P2DQ to efficiency scheduling mode. |
| `scx_tickless` | Lowers Tickless' housekeeping cadence to reduce wakeups and energy use. |

### Lowlatency

Favors minimal wakeup and queue latency with performance CPU mode and wakeup-preemption behavior.

Per-scheduler behavior:

| Scheduler | Behavior |
|---|---|
| `scx_lavd` | Enables LAVD's performance mode with a pinned 500 us slice for minimal latency. |
| `scx_bpfland` | Runs BPFland in performance mode with wakeup preemption. |
| `scx_cake` | Selects CAKE's esports profile for minimal sojourn latency. |
| `scx_cosmos` | Raises Cosmos' slice to 700 us with performance mode and wakeup preemption. |
| `scx_p2dq` | Enables P2DQ yield-on-idle and task-slice flags for fast dispatch. |
| `scx_tickless` | Applies Tickless' lower-latency frequency and slice settings. |

### Server

Favors sustained throughput with longer slices, strict CPU-affinity prioritization, and keep-running semantics.

Per-scheduler behavior:

| Scheduler | Behavior |
|---|---|
| `scx_lavd` | Runs LAVD in performance mode with 3000-10000 us slice bounds and a pinned 3000 us slice for sustained throughput. |
| `scx_bpfland` | Runs BPFland with a 20000 us slice and strict CPU-affinity prioritization. |
| `scx_p2dq` | Enables P2DQ keep-running behavior to keep loaded cores busy. |
| `scx_tickless` | Sets Tickless' housekeeping frequency for steady server operation. |

> The `scx_beerland` and `scx_pandemonium` Auto variants above are documented in the catalog for reference; the UI offers the profile selector only for the six profile-aware schedulers, so those two always run with their default behavior in the app.

## Requirements

Runtime:

- **A sched-ext capable kernel** — `CONFIG_SCHED_EXT` enabled. Verify with:

  ```sh
  grep SCHED_EXT /boot/config-$(uname -r)      # expect CONFIG_SCHED_EXT=y
  zcat /proc/config.gz | grep SCHED_EXT        # alternative, if the kernel exposes /proc/config.gz
  test -d /sys/kernel/sched_ext && echo yes    # runtime check — scx-manager reads scheduler state from here
  ```

- **`scx-tools`** (official Arch `extra`) — provides the `scx_loader` daemon (the `org.scx.Loader` D-Bus service) and `scxctl`, and pulls in **`scx-scheds`** (the scheduler binaries).
- **`qt6-base`** and **`polkit`** — plus a polkit agent available in your graphical session to present the approval dialog.

Building from source additionally needs a C++23 compiler (GCC 14+ or Clang 18), Qt6, CMake ≥ 3.20, Rust, and git — see the README's [Building from source](../README.md#from-source).

## Installation options

### From the AUR

`scx-manager` is published on the [Arch User Repository](https://aur.archlinux.org/packages/scx-manager):

```sh
# with any AUR helper (yay, paru, trizen, ...)
yay -S scx-manager
# or
paru -S scx-manager
```

Three package flavors install the same application — they conflict with each other, so only one can be present:

| Entry | Build model | Install |
|-------|-------------|---------|
| `scx-manager-bin` | Precompiled x86_64 binary — zero build dependencies (no cmake, no cargo, no CPM fetches) | `yay -S scx-manager-bin` |
| [`scx-manager`](https://aur.archlinux.org/packages/scx-manager) | Source build in your AUR chroot (full build toolchain) | `yay -S scx-manager` |
| [`scx-manager-git`](https://github.com/MadGoatHaz/scx-manager/tree/main/packaging/scx-manager-git) | Rolling build of the `main` branch (AUR submission pending — build from the repo) | `makepkg -si` in `packaging/scx-manager-git/` |

Pick the one that matches how you want it built:

- **Desktop users** → `scx-manager-bin`: one command, nothing compiled.
- **Reproducible or patched builds** → `scx-manager` (source): built from the release tarball in your AUR chroot with the full toolchain.
- **Latest unreleased fixes** → `scx-manager-git` (rolling `main`).

**Switching** is one command in either direction: the flavors declare symmetric conflicts, so `pacman -S scx-manager` (or `pacman -S scx-manager-bin`) removes the other and installs the chosen package in a single transaction — no state migration needed.

### Updating and uninstalling

Update as usual (`pacman -Syu`, or your AUR helper for the AUR flavors).

To uninstall, `pacman -Rns <flavor>`. Note that removing the package does **not** stop an active scheduler: the `scx_loader` daemon (from `scx-tools`) and `/etc/scx_loader.toml` persist until you disable the scheduler — via the app's **Disable** button or `scxctl stop`. With no SCX scheduler loaded, the kernel simply runs the default CFS scheduler, so the system always degrades gracefully.

## Troubleshooting

### "Cannot get information from scx_loader! Is it working?"

The `scx_loader` daemon is not present on the system bus. Fix:

1. Install the daemon and its tooling: `sudo pacman -S scx-tools` (pulls in `scx-scheds`).
2. Check the service: `systemctl status scx_loader`.
3. Verify the bus name is registered: `busctl --system list | grep org.scx.Loader`.
4. If needed, restart it: `sudo systemctl restart scx_loader` — then relaunch scx-manager.

### A scheduler I expected is missing from the list

The dropdown is whatever the **running** `scx_loader` version reports as supported, intersected with the scheduler binaries `scx-scheds` installed — compare it against `scxctl list` at any time. A scheduler binary present in `scx-scheds` may still be unsupported by an older daemon; update `scx-tools` and `scx-scheds` and check again. Conversely, a scheduler documented in this guide but absent from your list is simply not installed or not supported on your system — nothing is wrong with the app.

### Elevation (pkexec) fails

The `pkexec /usr/bin/cp` step is approved by the polkit agent of your graphical session. If no polkit agent is available (e.g. a headless session or an SSH session without a graphical agent), the approval dialog cannot be shown and the app reports the failure — it never silently escalates. Verify that `polkit` (and a polkit agent for your session) is installed; desktop sessions normally provide one automatically.

### The kernel has no sched_ext

If the checks in [Requirements](#requirements) show `CONFIG_SCHED_EXT` missing or `/sys/kernel/sched_ext` absent, no BPF scheduler can be loaded on this kernel — install a sched-ext capable kernel and reboot. In that state the app's current-scheduler indicator is empty and the daemon will not list any schedulers.

### "Cannot initialize scx_loader configuration"

`/etc/scx_loader.toml` exists but could not be parsed. Remove or repair the file and relaunch — when the file is absent the app falls back to the built-in default configuration.

### The daemon answers but the app cannot talk to it (API mismatch)

The bridge is built against the `scx_loader` 1.1.x D-Bus API (the `scx_loader` 1.1.2 crate). A much older daemon may fail to answer the supported-schedulers/current-scheduler queries; update `scx-tools` to the current version.

## The sched-ext ecosystem

- **sched-ext/scx** — the upstream project for the kernel scheduler class and its schedulers: <https://github.com/sched-ext/scx>
- **Arch packages** — `scx-tools` (<https://archlinux.org/packages/extra/x86_64/scx-tools/>), `scx-scheds` (<https://archlinux.org/packages/extra/x86_64/scx-scheds/>)
- **Kernel documentation** — `Documentation/sched_ext.rst` in the kernel tree, rendered at <https://docs.kernel.org/sched_ext.html>
- **Upstream home of the featured schedulers** — most are developed in the sched-ext/scx monorepo. Author attributions from the embedded catalog:
  - `scx_lavd` — Changwoo Min / Igalia
  - `scx_bpfland` — derived from `scx_rustland`
  - `scx_cake` — RitzDaCat
  - `scx_cosmos`, `scx_tickless`, `scx_beerland`, `scx_flash`, `scx_forge`, `scx_rustland` — Andrea Righi
  - `scx_p2dq` — Daniel Hodges (Meta)
  - `scx_flow` — Galih Tama
  - `scx_pandemonium` — William Clingan
  - `scx_rusty` — Tejun Heo
