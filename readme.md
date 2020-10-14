# RHODES

This repository demonstrates work by **Pengu Systems**. It includes Iris - A software application and algorithms for the [Nature Photonics publication for Wavefront shaping in complex media with a 350 kHz modulator via a 1D-to-2D transform](./docs/iris/s41566-019-0503-6.pdf)


Repository is structured as follows:
```bash
├── src
│   ├── libs
│   │   ├── core0
│   │   ├── ip_repo
│   │   ├── ...
│   ├── apps
│   │   ├── iris
│   │   ├── ...
├── ext
├── docs
│   ├── iris
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
