# Performance Testing and Profiling Guide

## Overview

This guide explains how to benchmark and profile the SNOW-V stream cipher implementations using Linux performance tools. We ensure consistent measurements by isolating execution to a single CPU core with a fixed frequency.

## Setting Up CPU Frequency Control and Isolation

To get accurate and repeatable performance measurements, we need to disable dynamic frequency scaling, lock the CPU to a fixed frequency, and isolate specific CPU cores from the kernel scheduler to prevent system interrupts and scheduling interference.

### Step 1: Edit GRUB Configuration

```bash
sudo nano /etc/default/grub
```

Add isolation parameters to your GRUB configuration. 

```bash
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash isolcpus=3"
```

### Step 2: Update GRUB

```bash
sudo update-grub
```
### Step 3: Reboot the System

After making these changes, reboot to apply the GRUB configuration:

```bash
sudo reboot
```

### Step 4: Set CPU Governor to Performance Mode

```bash
sudo cpupower frequency-set --governor performance
```

### Step 5: Check Available Frequency Range

```bash
cpupower frequency-info -o proc
```

### Step 6: Lock Specific CPU Core to a Fixed Frequency

Replace `3` with your desired CPU core and `4280000` with your target frequency:

```bash
taskset -c 3 echo 4280000 | sudo tee /sys/devices/system/cpu/cpu3/cpufreq/scaling_min_freq
```


After reboot, the specified CPU cores will be isolated and unavailable for normal kernel scheduling, and the system will run at a fixed frequency.

## Profiling with `perf`

We use `perf`, a powerful Linux profiling tool, to benchmark and analyze the performance of our implementations. Here's how to use it:

### Compilation

Compile your code with the `-g` flag to enable symbol resolution and debugging information:

```bash
gcc -g SNOW-V64.cpp -o snowv64
```

### Recording Performance Data

Use `perf record` to collect execution data:

```bash
sudo taskset -c 3 perf record --call-graph dwarf ./snowv64_30b
```

This command:
- Runs on CPU core 3 (`taskset -c 3`)
- Records performance metrics (`perf record`)
- Captures call graphs with DWARF debug info (`--call-graph dwarf`)

### Analyzing Performance Data

Use `perf report` to identify which functions consume the most execution time:

```bash
sudo perf report
```

This will show a breakdown of CPU time spent in each function, helping you identify performance bottlenecks.

