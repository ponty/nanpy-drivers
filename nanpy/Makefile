TARGET = nanpy



ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m	:= $(TARGET).o

else

# out-of-tree build
KDIR ?= /lib/modules/`uname -r`/build
PWD       := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

endif

clean:
	rm -rf *.o *.ko *~ core .depend *.mod.c .*.cmd .tmp_versions .*.o.d

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend

ins: default rem
	insmod $(TARGET).ko 
#	insmod $(TARGET).ko debug=1

rem:
	@if [ -n "`lsmod | grep -s $(TARGET)`" ]; then rmmod $(TARGET); echo "rmmod $(TARGET)"; fi

ifeq (.depend,$(wildcard .depend))
include .depend
endif
