#!/bin/sh
wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.13.tar.xz
tar -xJf linux-3.13.tar.xz
cd linux-3.13 
make defconfig
make oldconfig
make prepare
make modules_prepare
cd .. 

