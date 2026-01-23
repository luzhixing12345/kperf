obj-m := hello_world.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all: userspace
	make -C $(KDIR) M=$(PWD) modules

userspace: userspace.o
userspace.o: userspace.c

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f userspace