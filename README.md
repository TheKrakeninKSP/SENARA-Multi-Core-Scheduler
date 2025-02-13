# SENARA-Multi-Core-Scheduler

An asymmetric multi-core process scheduler with a testbench and customizable parameters.

## Overview
SENARA is a **multi-processor scheduler** implementing an asymmetric scheduling algorithm. It categorizes CPU cores into three specialized types to optimize performance and power usage.

## Core Types

- **Long Process (LP) Cores**:
  - Handle processes with large burst times, ideal for background algorithms.
  - High compute power, high power usage.

- **Fast Response (FR) Cores**:
  - Preferably run GUI front-ends and interactive processes.
  - Lower compute power, low power usage.

- **General Purpose (GP) Cores**:
  - Balanced power and performance.

## Motivation

Symmetric multiprocessing (SMP) has **load imbalance issues** ([Wikipedia](https://en.wikipedia.org/wiki/Symmetric_multiprocessing)). SENARA follows an **asymmetric approach**, inspired by ARM **big.LITTLE** architecture ([Wikipedia](https://en.wikipedia.org/wiki/ARM_big.LITTLE)), utilizing **core specialization** for efficient scheduling.

## Architecture

### Memory Model
- **Shared main memory** via a **common system bus**.
- **Separate cache per core**.
- **Cache coherence** handled by the **system bus**.

### Scheduling Queues
- **Global Queue**: All new processes arrive here initially.
- **LP Buffer**: Holds long processes before assigning them to LP cores.
- **FR Buffer**: Holds interactive processes before assigning them to FR cores.

### Scheduling Algorithm
1. Assign processes in **LP Buffer** to **LP cores**.
2. Assign processes in **FR Buffer** to **FR cores**.
3. Assign processes from **Global Queue** to the **GP core** with the least number of processes in its queue.
4. Repeat steps 1–3 until all processes terminate.

## Process Migration

### Long Process Detection
- Track **cumulative burst time** for each process.
- If a process **exceeds `LONG_PROC_THRESH`**, move it to **LP Buffer**.

### Interactive Process Detection
- Compute **interaction ratio** = (Number of **voluntary I/O preemptions**) / (Cumulative **burst time**).
- If the **interaction ratio is high**, move the process to **FR Buffer**.

## Power Modes

| Mode           | Behavior |
|---------------|----------|
| **Power-Saving** | Prioritizes GP cores, minimizes LP and FR cores usage. |
| **Balanced** | Even distribution across cores, but not all cores are active. |
| **Performance** | All cores active, maximum power usage. |

## Implementation Details
- **Time Slice**: Uses **Win32 standard** (~20ms per slice).
- **Cache Coherence**: Handled by the system bus.

## References
- [Symmetric Multiprocessing (SMP)](https://en.wikipedia.org/wiki/Symmetric_multiprocessing)
- [ARM big.LITTLE Architecture](https://en.wikipedia.org/wiki/ARM_big.LITTLE)

## Future Enhancements
- **Dynamic Core Allocation**: Adjust core assignment based on system load.
- **Energy Efficiency Optimization**: Implement **Dynamic Voltage and Frequency Scaling (DVFS)**.
- **Load Prediction**: Use **historical data** or **machine learning** for proactive scheduling.
- **Cache Optimization**: Introduce a **cache-aware scheduler** to minimize cache misses.
- **Preemptive vs Non-Preemptive Scheduling**: Define conditions for process preemption based on priority, starvation, or core availability.

