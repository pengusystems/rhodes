## Microzed

### Vivado
Each vivado project should have a .tcl file to recreate it. In order to recreate a project using the .tcl file, use the following steps:
1. Launch vivado
2. from the startup screen tcl console, cd to this directory and source xxxxx.tcl
3. Create the hardware with vivado
4. Export xsa file (this is actually a zip file) including the bitstream to the main project folder.


### Petalinux
#### To create a valid linux domain:
1. In windows, create a boot folder with the following files which can be obtained from the petalinux project under `images/linux/`
  * zynq_fsbl.elf
  * image.ub
  * u-boot.elf
  * system.bit
2. In windows, use a bif file like the one in this folder (can also find something like that in petalinux build folder)
3. For the sysroot libraries, in petalinux run petalinux-build -sdk, then run sdk.sh and export to some folder (requested by `sdk.sh`). 
  This will create a sysroots folder for cross compiling (cortexa9t2hf-neon-xilinx-linux-gnueabi)
4. Bring the cross compiling folder to windows (need to copy and resolve symbolic link `cp -Lr`) and point vitis to `x86_64-petalinux-linux`


#### To create a valid linux app:
1. Add a linux application to the selected domain
2. If the GUI for setting up the application does not display correctly, change the display scale to 100% (windows setting)


#### DHCP and networking for debugging an app:
1. By default, tcf-agent runs on petalinux build (ps | grep tcf)
2. If microzed is connected to a router or a [PC running DHCP server](https://support.atlona.com/hc/en-us/articles/115001288894-KB01257-How-to-turn-your-computer-Windows-into-a-DHCP-server-to-give-your-Atlona-unit-an-IP-address), u-boot will request IP automatically
3. For direct connection to PC which is not running a DHCP server, need to set ipv4 and subnet mask on microzed to match the pc one (pay attention to which ethernet adapter is used on pc)
  `ifconfig eth0 <new ip address> netmask <new subnet mask>`    for example: `ifconfig eth0 169.254.9.85 netmask 255.255.0.0`
4. If PC is running a DHCP server, ip address will be obtained automatically


#### Workflow (on VM with petalinux):
SSH into VM: `ssh pengu@127.0.0.1 -p 2222` (password `1234`)
```
source /opt/pkg/petalinux/2019.2/settings.sh
./project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi
petalinux-config -c kernel
petalinux-config -c u-boot
petalinux-config -c rootfs
petalinux-build
petalinux-package --boot --u-boot --fsbl zynq_fsbl.elf  --fpga system.bit
petalinux-package --sysroot --sdk which will create sdk.sh that will install the sysroot
```

#### Workflow (on board):
SSH into board: `ssh root@192.168.11.119` (password is `root`)
setup DHCP server, such that board can get an IP.
`ssh root@192.168.11.119` to work on the machine from a windows power shell
to disable ssh strict checking create a config file (no extension) in `/users/.ssh/` which looks like this:
```
Host 192.168.11.119
   StrictHostKeyChecking no
   UserKnownHostsFile=/dev/null
```
