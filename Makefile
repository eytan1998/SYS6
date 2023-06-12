obj-m += chardev.o

PWD := $(CURDIR)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc main.c -o main

clean:
	rm -f main
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
