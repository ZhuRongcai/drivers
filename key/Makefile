CURRENT_DIR := $(shell pwd)
TOP_DIR := $(CURRENT_DIR)/../..
KERNEL_DIR := $(TOP_DIR)/kernel
DRV_DIR := $(TOP_DIR)/drv

KBUILD_EXTRA_SYMBOLS += $(DRV_DIR)/led/Module.symvers
export KBUILD_EXTRA_SYMBOLS

obj-m := key.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C  $(KERNEL_DIR) M=$(CURRENT_DIR) modules
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURRENT_DIR) clean
