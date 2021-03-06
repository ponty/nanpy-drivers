=============
nanpy-drivers
=============

Drivers for [Nanpy firmware](https://github.com/nanpy/nanpy-firmware) + [Arduino](http://www.arduino.cc/).

[![Build Status](https://travis-ci.org/ponty/nanpy-drivers.svg)](https://travis-ci.org/ponty/nanpy-drivers)

Using this experimental Linux driver the Arduino board can be used as an I2C expander 
(like [i2c_tiny_usb](http://www.harbaum.org/till/i2c_tiny_usb/)) or as a GPIO expander.
You can control GPIO pins using the [Linux GPIO interface](https://www.kernel.org/doc/Documentation/gpio/sysfs.txt),
or you can connect I2C devices to the Arduino and use its kernel driver.
(PCF8574 was tested only.)

Future plans: 
 - add 1-Wire driver
 - add hwmon driver
 - split driver into more modules
 - dynamic query of the Arduino

Build 
=====

Test system: Ubuntu 14.04.

    sudo apt-get install linux-image-extra-virtual i2c-tools build-essential
    make
    

Testing GPIO
============

Turning on pin D13:

    echo 255 | sudo tee /sys/class/gpio/export
    echo out | sudo tee /sys/class/gpio/gpio255/direction
    echo 1 | sudo tee /sys/class/gpio/gpio255/value


Testing I2C
===========

    sudo modprobe i2c-dev
    sudo insmod nanpy/nanpy.ko
    sudo nanpyattach/nanpyattach /dev/ttyS0 &
    
    dmesg

    serio: Serial port ttyS0
    nanpy serio2: Sleeping 2000 msec..
    nanpy serio2: Command buffer: Wire 0 0 begin 
    nanpy serio2: Answer buffer: 0
    nanpy serio2: Connected to nanpy
    
    ls /sys/module/nanpy/drivers/serio:nanpy/serio*/|grep i2c
        i2c-5
    
    i2cdetect -l
        ...
        i2c-5   unknown     nanpy on ttyS0/serio0               N/A
    
    sudo i2cdetect -y 5
             0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
        00:          -- -- -- -- -- -- -- -- -- -- -- -- -- 
        10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
        20: -- -- -- -- -- -- -- 27 -- -- -- -- -- -- -- -- 
        30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
        40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
        50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
        60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
        70: -- -- -- -- -- -- -- 77

Testing PCF8574 on I2C bus
==========================

    cd /sys/module/nanpy/drivers/serio:nanpy/serio*/i2c-*/
    echo pcf8574 0x27  | sudo tee new_device
       
dmesg:
 
    i2c i2c-5: new_device: Instantiated device pcf8574 at 0x27
    nanpy serio3: Command buffer: Wire 0 3 requestFrom 39 1 1 
    nanpy serio3: Answer buffer: 1
    nanpy serio3: Command buffer: Wire 0 0 read 
    nanpy serio3: Answer buffer: 247
    pcf857x 5-0027: probed    

setting GPIO pin 4 direction and value: (backlight on my LCD interface)    
 
    echo 237 | sudo tee /sys/class/gpio/export
    echo out | sudo tee /sys/class/gpio/gpio237/direction
    echo 0 | sudo tee /sys/class/gpio/gpio237/value
    echo 1 | sudo tee /sys/class/gpio/gpio237/value

Removing driver
===============
    
Unexport all GPIO (important!)

    echo 237 | sudo tee /sys/class/gpio/unexport
    echo 255 | sudo tee /sys/class/gpio/unexport
    
close nanpyattach:

    sudo killall nanpyattach
    
remove module:

    sudo rmmod nanpy        
    
