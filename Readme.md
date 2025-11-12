# High-throughput 64-bit implementation of SNOW-V stream cipher — Implementation and profiling

This repository contains the implementation and performance results accompanying the paper:

"High throughput 64-bit implementation of SNOW-V stream cipher"

Authors: Kakumani Kushalram, Majji Harsha Vardhan, Raghvendra Rohit

To appear in: SPACE 2025

The codebase includes 32-bit and 64-bit implementation sources and the benchmarking/profiling guidance used to generate the results reported in the paper.

Repository layout (important files):

- `src/SNOW-V32.cpp` — 32-bit implementation used for comparison.
- `src/SNOW-V64.cpp` — 64-bit high-throughput implementation (primary focus).

Below is the performance testing and profiling guide used during evaluation. It documents the steps we followed to get consistent, repeatable measurements on Linux systems.

## Performance Testing and Profiling Guide

### Overview

This guide explains how to benchmark and profile the SNOW-V stream cipher implementations using Linux performance tools. We ensure consistent measurements by isolating execution to a single CPU core with a fixed frequency.

### Setting Up CPU Frequency Control and Isolation

To get accurate and repeatable performance measurements, we disable dynamic frequency scaling, lock the CPU to a fixed frequency, and isolate specific CPU cores from the kernel scheduler to reduce system interrupts and scheduling interference.

1. Edit GRUB configuration (example):

```bash
sudo nano /etc/default/grub
```

Add isolation parameters to your GRUB configuration. For example:

```bash
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash isolcpus=3"
```

2. Update GRUB and reboot:

```bash
sudo update-grub
sudo reboot
```

3. Set the CPU frequency governor to performance mode:

```bash
sudo cpupower frequency-set --governor performance
```

4. (Optional) Check available frequency range:

```bash
cpupower frequency-info -o proc
```

5. Lock a specific CPU core to a fixed minimum frequency (replace `3` with the core and `4280000` with the desired frequency in kHz):

```bash
taskset -c 3 echo 4280000 | sudo tee /sys/devices/system/cpu/cpu3/cpufreq/scaling_min_freq
```

After reboot, the specified CPU cores will be isolated and the system can be run at a fixed frequency for consistent benchmarking.

### Profiling with `perf`

We use `perf` to record and analyze CPU performance. Build with debug symbols to get call-graph information.

Compile (example):

```bash
g++ -g src/SNOW-V64.cpp -o snowv64
```

Record a run with call graphs (example, running on isolated core 3):

```bash
sudo taskset -c 3 perf record --call-graph dwarf ./snowv64
```

Then analyze the results:

```bash
sudo perf report
```

`perf report` will show which functions consume the most CPU time and help identify hotspots to optimize.



