ARCH=arm
obj-m := drv.o

KERNELDIR := /home/giohkim/work/kernel/linux-lg115x
PWD := $(shell pwd)

default :
	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) EXTRA_CFLAGS="-I$(KERNELDIR)"

clean:
	rm -rf *.ko
	rm -rf *.mod.*
	rm -rf .*.cmd
	rm -rf *.o
	rm -f *.markers
	rm -f *.order
	rm -f *.symvers
	rm -rf .tmp_versions
