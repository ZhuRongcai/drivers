CURRENT_DIR := $(shell pwd)

KERNEL_DIR := $(CURRENT_DIR)/../../kernel
obj-m := led.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURRENT_DIR) modules
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURRENT_DIR) clean
