# HASAC: Energy Adaptive Secure Firmware Updates for Critical IoT Systems

This repository contains artefacts and proof-of-concept code of the paper `HASAC`, which will appear at [IFIP SEC 2026](https://ifipsec.org/).

> **Abstract:** Secure Firmware Over-The-Air (FOTA) updates are essential for Critical Infrastructure IoT (CI-IoT) devices but impose severe energy costs on battery-powered nodes operating over Low Power Wide Area Networks such as NB-IoT. This paper presents HASAC (*Hardware-Aware Segmented Authenticated Compression*), a FOTA framework for resource-constrained IoT devices that minimises update energy by dynamically adapting cryptographic and compression strategies to device hardware capabilities and radio conditions.
We implement HASAC on a Cortex-M33 platform and show that radio transmission and interconnect overhead account for more than 90\% of the total update energy, while cryptographic computation contributes only a small fraction. We further demonstrate that on devices equipped with hardware security modules, AES-128-CCM achieves more than an order-of-magnitude improvement in throughput and energy efficiency compared to software implementations, even for small firmware segments.
Building on these insights, HASAC dynamically selects between hardware-accelerated AES and software-optimised ChaCha20-Poly1305, and applies link-aware LZ4 compression only when it yields a positive energy return. Model-based evaluation demonstrates that, in deep coverage scenarios, HASAC reduces the total energy cost of secure firmware updates by up to 30\% compared to conventional uncompressed baselines, reconciling strong security guarantees with long-term battery longevity.

The code is provided to reviewers for **full reproducibility** and **independent verification** of our implementation claims. It targets the **nRF5340 DK (Arm Cortex-M33)** running the **nRF Connect SDK (NCS)** and measures protocol costs.

---

## Quick start

Install the official Nordic toolchain (the evaluation was done with these versions):

- **nRF Connect SDK v3.0.0**
- **Zephyr OS v4.0.99**

Make sure you have the target board connected to the computer, and then run.

```bash
# 1. Initialise west in a separate workspace (if you do not already use NCS)
west init -m https://github.com/nrfconnect/sdk-nrf --mr v3.0.0 ncs
cd ncs
west update

# 2. Export Zephyr environment (one shell per terminal)
west zephyr-export
source zephyr/zephyr-env.sh

# 3. After installation, ensure the following commands work in a fresh shell
west --version
cmake --version
ninja --version
arm-none-eabi-gcc --version

# 4. Download HASAC in a sibling directory (the zip file)
cd ..
cd hasac
west build -b nrf5340dk/nrf5340/cpuapp --pristine
west flash
```

You should also have the `zephyr-env` script set up (for example via source `~/ncs/v3.0.0/zephyr/zephyr-env.sh` or the equivalent `path` on your system). If you see any `nrfjprog` or `nrftool` errors, please ensure that the Nordic command line tools are installed and the board is listed by `nrfjprog --ids`. If you have a different Nordic board, replace the build line with your board name and version.

To view the results, open a serial terminal to view the benchmark output. You can use any terminal emulator (PuTTY, minicom) or the built-in `west` command:

```bash
# 115200 baud, 8N1 via the West command-line
west espressif monitor  # (or your preferred serial tool)

# macOS
screen /dev/tty.usbmodem* 115200

# Linux
screen /dev/ttyACM0 115200

# Windows
Use PuTTY (Serial connection type, line: COM3, speed: 115200 baud)

# Native VS Code
Use UART viewer (VCOM1)
```

## Benchmark results

Below is the exact benchmark output captured from our nRF5340 DK running this firmware. These numbers correspond to a Cortex M33. Hardware AES-128-CCM measurements use the Arm CryptoCell-312 accelerator via PSA Crypto, as integrated in the nRF5340.

```text
==============================================================================
      HASAC BENCHMARK SUITE (nRF5340 / Cortex M33)
      Payload: 1024 bytes | Iters: 100 | Power: 30 mW | Clock: 32768 Hz
==============================================================================
| Algorithm          |  Time (ms)  |  Energy(uJ)  | CPU Cycles|  Speed(kB/s) |
|--------------------|-------------|--------------|-----------|--------------|
| SW AES-128-CCM     |       9.637 |       289.11 |    616777 |       103.76 |
| SW ChaCha Poly     |       1.589 |        47.68 |    101718 |       629.18 |
| HW AES-128-CCM     |       0.361 |        10.83 |     23105 |      2769.90 |
| SW LZ4 Decompress  |       0.022 |         0.67 |      1445 |     44281.08 |
==============================================================================
```

## Directory Strcuture

```bash
.
├── build/                      # Build artifacts (generated)
├── CMakeLists.txt              # CMake build configuration
├── prj.conf                    # Kconfig configuration (Crypto & System)
├── Real Smart Water Meter update files
│   ├── 1kSegments              # Real-world update file data of 1kB segments
│   └── 8kSegments              # Real-world update file data of 8kB segments
└── src/
    ├── main.c                  # Benchmarking suite and all crypto codes
    ├── lz4.c                   # LZ4 compression algorithm
    └── lz4.h                   # LZ4 compression definitions
```

## Citation

Full version of the paper can be found [here](https://cosicdatabase.esat.kuleuven.be/backend/publications/files/conferencepaper/4159). If you find this work useful, please consider citing the paper:

```bibtex
@inproceedings{HASAC,
    author    = {Sayon Duttagupta},
    title     = {{HASAC: Energy Adaptive Secure Firmware Updates for Critical IoT Systems}},
    booktitle = {Proceedings of the 41st International Conference on ICT Systems Security and Privacy Protection (IFIP SEC 2026)},
    year      = {2026},
    doi       = {},
    note      = {To appear}
}
```

## Acknowledgments

This work was supported in part by the Flemish Government through the Cybersecurity Research Program with grant number VOEWICS02, and by the European Union’s Horizon Research and Innovation program under grant agreement No.\ 101119747 (TELEMETRY).
