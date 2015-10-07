ARCH=arm
obj-m := my_brd.o

KERNELDIR := /lib/modules/3.19.0-25-generic/build
#KERNELDIR := /home/giohkim/work/kernel/linux-lg115x
PWD := $(shell pwd)

default :
	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD)

clean:
	rm -rf *.ko
	rm -rf *.mod.*
	rm -rf .*.cmd
	rm -rf *.o
	rm -f *.markers
	rm -f *.order
	rm -f *.symvers
	rm -rf .tmp_versions
