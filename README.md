# scx-manager
Simple GUI for managing sched-ext schedulers via scx_loader.

Ported from the upstream scx-manager project.

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

Requirements
------------
* C++23 feature required (tested with GCC 14.1.1 and Clang 18)
Any compiler which support C++23 standard should work.

######
## Installing from source

This is tested on Arch Linux, but *any* recent Arch Linux based system with latest C++23 compiler should do:

```sh
sudo pacman -S \
    base-devel cmake make qt6-base qt6-tools polkit-qt6 python
```

### Cloning the source code
```sh
git clone <scx-manager-repository-url>
cd scx-manager
```

### Building and Configuring
To build, first, configure it(if you intend to install it globally, you
might also want `--prefix=/usr`):
```sh
./configure.sh --prefix=/usr/local
```
Second, build it:
```sh
./build.sh
```


### Libraries used in this project

* [Qt](https://www.qt.io) used for GUI.
* [A modern formatting library](https://github.com/fmtlib/fmt) used for formatting strings, output and logging.
* [sched-ext](https://github.com/sched-ext/scx) kernel scheduling interface used by scx_loader.
