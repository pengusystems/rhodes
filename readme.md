# rhodes

This repository demonstrates work by **Pengu Systems**. It includes work in C++, MATLAB and HDL.<br>
Work of interest:
* iris - Software application and algorithms for the [Nature Photonics publication for Wavefront shaping in complex media with a 350 kHz modulator via a 1D-to-2D transform](./docs/iris/s41566-019-0503-6.pdf)
* microzed - Embedded programming for Xilinx Zynq7000 SoC. Includes:
    * A bare metal app [birch](./src/embedded/microzed/sw/nile/birch) which demonstrates communication between the PL & PS and DMA for a custom dummy axi4 stream ip to DDR
    * A linux app [acacia](./src/embedded/microzed/sw/nile/acacia) which demonstrates gpio access using sysfs & mmio on two seperate threads
* fmcw_radar - [MATLAB notebook (live editor)](./docs/fmcw_radar/) that explains the signal processing done in typical FMCW radars


Repository is structured as follows:
```bash
├── src
│   ├── libs
│   │   ├── core0
│   │   ├── ip_repo
│   │   ├── ...
│   ├── embedded
│   │   ├── microzed
│   │   │   ├── fw
│   │   │   │   ├── nile
│   │   │   ├── sw
│   │   │   │   ├── nile
│   │   │   │   │   ├── acacia
│   │   │   │   │   ├── birch
│   │   ├── ...
│   ├── apps
│   │   ├── iris
│   │   ├── ...
├── ext
├── docs
│   ├── iris
│   ├── fmcw_radar
│   ├── ...
├── scripts
```
* src - our code
  * libs - shared code (firmware & software)
  * embedded - focused on embedded systems. ordered by boards, where each board could have the following directories:
    * fw - stores firmware by name of the project (rivers)
    * sw - subdivided based on optional firmwares, where each firmware contains the names of the software apps (trees)
    * hw - board design files
  * apps - pc apps by name of project (flowers)
* ext - external libraries & tool
* docs - documentation, no stricts rules
* scripts - deploy and extract scripts

In addition, pc apps and libraries (libs) will deploy into the root in one of the following forms:
* `Debug_x64`
* `Release_NoOpt_x64`
* `Release`