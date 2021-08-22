obj-m += soa.o dev.o
soa-objs += ./lib/usctm.o ./lib/vtpmo.o ./lib/tag.o ./lib/level.o
dev-objs += ./lib/driver.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean