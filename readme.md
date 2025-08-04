# RHODES

A software application and algorithms for the [Nature Photonics publication for Wavefront shaping in complex media with a 350 kHz modulator via a 1D-to-2D transform](https://www.nature.com/articles/s41566-019-0503-6). Also appears locally [here](./docs/iris/s41566-019-0503-6.pdf)


Repository is structured as follows:
```bash
├── src
│   ├── libs
│   │   ├── core0
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
  * libs - shared code
  * apps - applications
* ext - external libraries
* docs - documentation
* scripts - deployment scripts

In addition, pc apps and libraries (libs) will deploy into the root in one of the following forms:
* `Debug_x64`
* `Release_NoOpt_x64`
* `Release`
