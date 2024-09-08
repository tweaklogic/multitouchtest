MultiTouchTest
=======

### Purpose
This utility tests multi-point touchscreens with DRM/KMS display backend.
This tool is developed to assess the DRM/KMS functionality and touchscreens when application stack is not ready.

It is useful for board bringups, touchscreen or touchpad driver development or display panel driver development.

Display panels can be HDMI, LVDS, MIPI, etc. and touch devices could be any input device such as USB, I2C, SPI based multi-point touch solutions as long as the driver is developed to integrate with the Linux input layer.

STM32MP157C-DK2 and STM32MP157C-EV1 boards were used for initial development. 

### Usage
Each touch displays circles with red, green, blue and white color respectively. You can drag each circle independently.

[![multitouchtest](https://img.youtube.com/vi/TyvByuKKVMI/0.jpg)](https://youtu.be/TyvByuKKVMI)

### Main blog
[multitouchtest](https://www.tweaklogic.com/touchscreen-tester-with-drm-kms-backend/)

### Compile libdrm

This project depends on **libdrm** which requires **meson** and **ninja**.
```
PC $> sudo apt install meson
PC $> sudo apt install ninja-build
```

Source you cross-compile toolchain and verify ***$CC***:
```
PC $> source ../../toolchain_mickledore-mpu-v24.06.26/environment-setup-cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi

PC $> echo $CC
arm-ostl-linux-gnueabi-gcc -mthumb -mfpu=neon-vfpv4 -mfloat-abi=hard -mcpu=cortex-a7 --sysroot=/home/subhajit/opensource_contributions/toolchain_mickledore-mpu-v24.06.26/sysroots/cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi
```

Download and extract [libdrm](https://gitlab.freedesktop.org/mesa/drm).
Go to tags and download the latest tagged version or git clone it, your wish!
At the time of writing, latest version was: drm-libdrm-2.4.123 
```
PC $> tar -xf drm-libdrm-2.4.123.tar.bz2
```


Compile and install **libdrm**. If a cross-compiler toolchain is sourced, meson automatically picks it up. No need to perform any additional steps.

```
PC $> cd drm-libdrm-2.4.123/
PC $> meson builddir --prefix=$PWD/builddir/output
PC $> ninja -C builddir/ install
```

If something goes wrong, you can wipe the build:
```
PC $> meson setup --wipe builddir/
PC $> rm -rf builddir/
```

After the build is successful or you have your cross compiled libdrm libraries set up, they should look like this:
```
PC $> tree builddir/output/
builddir/output/
├── include
│   ├── libdrm
│   │   ├── amdgpu_drm.h
│   │   ├── amdgpu.h
│   │   ├── drm_fourcc.h
│   │   ├── drm.h
│   │   ├── drm_mode.h
│   │   ├── drm_sarea.h
│   │   ├── i915_drm.h
│   │   ├── mach64_drm.h
│   │   ├── mga_drm.h
│   │   ├── msm_drm.h
│   │   ├── nouveau
│   │   │   ├── nouveau.h
│   │   │   └── nvif
│   │   │       ├── cl0080.h
│   │   │       ├── cl9097.h
│   │   │       ├── class.h
│   │   │       ├── if0002.h
│   │   │       ├── if0003.h
│   │   │       ├── ioctl.h
│   │   │       └── unpack.h
│   │   ├── nouveau_drm.h
│   │   ├── qxl_drm.h
│   │   ├── r128_drm.h
│   │   ├── r600_pci_ids.h
│   │   ├── radeon_bo_gem.h
│   │   ├── radeon_bo.h
│   │   ├── radeon_bo_int.h
│   │   ├── radeon_cs_gem.h
│   │   ├── radeon_cs.h
│   │   ├── radeon_cs_int.h
│   │   ├── radeon_drm.h
│   │   ├── radeon_surface.h
│   │   ├── savage_drm.h
│   │   ├── sis_drm.h
│   │   ├── tegra_drm.h
│   │   ├── vc4_drm.h
│   │   ├── via_drm.h
│   │   ├── virtgpu_drm.h
│   │   └── vmwgfx_drm.h
│   ├── libsync.h
│   ├── xf86drm.h
│   └── xf86drmMode.h
├── lib
│   ├── libdrm_amdgpu.so -> libdrm_amdgpu.so.1
│   ├── libdrm_amdgpu.so.1 -> libdrm_amdgpu.so.1.123.0
│   ├── libdrm_amdgpu.so.1.123.0
│   ├── libdrm_nouveau.so -> libdrm_nouveau.so.2
│   ├── libdrm_nouveau.so.2 -> libdrm_nouveau.so.2.123.0
│   ├── libdrm_nouveau.so.2.123.0
│   ├── libdrm_radeon.so -> libdrm_radeon.so.1
│   ├── libdrm_radeon.so.1 -> libdrm_radeon.so.1.123.0
│   ├── libdrm_radeon.so.1.123.0
│   ├── libdrm.so -> libdrm.so.2
│   ├── libdrm.so.2 -> libdrm.so.2.123.0
│   ├── libdrm.so.2.123.0
│   └── pkgconfig
│       ├── libdrm_amdgpu.pc
│       ├── libdrm_nouveau.pc
│       ├── libdrm.pc
│       └── libdrm_radeon.pc
```

### Compile multitouchtest

```
PC $> cd ../multitouchtest
PC $> $CC multitouchtest.c -o multitouchtest -I../drm-libdrm-2.4.123/builddir/output/include/ -I../drm-libdrm-2.4.123/builddir/output/include/libdrm/ -L../drm-libdrm-2.4.123/builddir/output/lib/ -ldrm -lm -lpthread
```

### Verify and scp to target board
```
PC $> file multitouchtest
multitouchtest: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-armhf.so.3, BuildID[sha1]=51489fac1dc10af619ce0449d894099d39677e9b, for GNU/Linux 3.2.0, with debug_info, not stripped

PC $> scp multitouchtest 192.168.7.1:~/
```


### Run multitouchtest on STM32MP157C-DK2

***Don't forget to turn off Display Manager if you are just testing this on a Starter package with full Desktop support:***

```
BOARD #> systemctl stop display-manager
```

Help:
```
BOARD #> ./multitouchtest -h
-s Show all the DRM and Event options
-e [Event index] Use the Event device index
-d [DRM index] Use the DRM device index

```

Show available input devices and displays:
```
BOARD #> ./multitouchtest -s
Input devices:
Index	Name		Driver
0	EP0110M09	1.0.1
1	pmic_onkey	1.0.1

DRM details:
stm module in use
Index	Connectors	Modes		Possible CRTCs
0	34		480x800		41 

```

Use index 0 for touchscreen input and index 0 as well for display (Ctrl-C to exit):
```
BOARD #> ./multitouchtest -e 0 -d 0
DRM details:
stm module in use
Index	Connectors	Modes		Possible CRTCs
0	34		480x800		41 

Using Connector ID:34, CRTC ID:41
```

### Run multitouchtest on STM32MP157C-EV1
***Again, don't forget to turn off Display Manager:***

```
BOARD #> systemctl stop display-manager
```

Show available input devices and displays:
```
BOARD #> ./multitouchtest -s
Input devices:
Index	Name		Driver
0	Goodix Capacitive TouchScreen	1.0.1
1	pmic_onkey	1.0.1
2	joystick	1.0.1

DRM details:
stm module in use
Index	Connectors	Modes		Possible CRTCs
0	32		720x1280		38 
```

Use index 0 for touchscreen input and index 0 as well for display (Ctrl-C to exit):
```
BOARD #>  ./multitouchtest -e 0 -d 0
DRM details:
stm module in use
Index	Connectors	Modes		Possible CRTCs
0	32		720x1280		38 

Using Connector ID:32, CRTC ID:38
```

### TODO

Somehow Raspberry Pi implementation is broken and throws a runtime mmap error.
Compilation was successful on the target.
Send me a pull request if you manage to fix it!