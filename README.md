MultiTouchTest
=======

## Purpose
This utility tests multipoint touchscreens with DRM/KMS display backend.
This tool is developed to assess the DRM/KMS functionality and the touchscreen when no display infrastructure is up.

It is useful for board bringups, touchscreen or touchpad driver developement or display panel driver development.

Display panels can be HDMI, LVDS, MIPI, etc. and touch devices could be any input device such as USB, I2C, SPI based multipoint touch soultions as long as the driver is developed to integrate with the Linux input layer.

STM32MP157-DK2 SBC is used for initial development of this test tool. 

## Usage
Touching fingers on the touchscreen displays circles with red, green, blue and white color respectively. You can drag each circle indendently.

Module "stm" is hard coded. Please change accoring to your board

## Build requirments
Libdrm

## Compilation
```
gcc multitouchtest.c -o multitouchtest -I/usr/include/drm/ -ldrm -lm -lpthread
```
```
<cross-compiler-prefix>-gcc multitouchtest.c -o multitouchtest -I<libdrm_inc_path> -I<libdrm_inc_path>/drm -L<libdrm_lib_path> -ldrm -lm -lpthread
```
