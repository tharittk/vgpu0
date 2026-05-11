QEMU_PATH = ../env/qemu-11.0.0
QEMU_BUILD_DIR = $(QEMU_PATH)/build
QEMU_SRC_FILE = vgpu0-dev.c
QEMU_DEST_DIR = $(QEMU_PATH)/hw/misc

.PHONY: all dev qcopy qpatch qbuild qclean

all: dev drv

dev: qcopy qpatch qbuild

qcopy:
	@echo "Copying QEMU source..."
	cp $(QEMU_SRC_FILE) $(QEMU_DEST_DIR)/

qpatch:
	@echo "Patching QEMU Kconfig and meson.build..."
	@if ! grep -q "config CUSTOM_PCI_DEV" $(QEMU_DEST_DIR)/Kconfig; then \
		echo "\nconfig CUSTOM_PCI_DEV\n    bool\n    default y\n    depends on PCI" >> $(QEMU_DEST_DIR)/Kconfig; \
	fi
	@if ! grep -q "$(QEMU_SRC_FILE)" $(QEMU_DEST_DIR)/meson.build; then \
		echo "system_ss.add(when: 'CONFIG_CUSTOM_PCI_DEV', if_true: files('$(QEMU_SRC_FILE)'))" >> $(QEMU_DEST_DIR)/meson.build; \
	fi

qbuild:
	@echo "Building QEMU..."
	mkdir -p $(QEMU_BUILD_DIR)
	cd $(QEMU_BUILD_DIR) && meson setup .. && ninja

qclean:
	rm -rf $(QEMU_BUILD_DIR)


KERNEL_PATH = ../env/linux-7.0.3/
KERNEL_SRC_FILE = vgpu0-drv.c
KERNEL_DEST_DIR = $(KERNEL_PATH)/drivers/leds/
# Strip .c to get the object name for Makefile
KERNEL_OBJ = $(KERNEL_SRC_FILE:.c=.o)

.PHONY: drv kcopy kpatch kbuild kclean

drv: kcopy kpatch kbuild

kcopy:
	@echo "Copying Kernel driver source..."
	cp $(KERNEL_SRC_FILE) $(KERNEL_DEST_DIR)/

kpatch:
	@echo "Patching Kernel Kconfig and Makefile..."
	@# Add entry to Kconfig
	@if ! grep -q "config CUSTOM_VGPU_DRV" $(KERNEL_DEST_DIR)/Kconfig; then \
		echo "\nconfig CUSTOM_VGPU_DRV\n    tristate \"Virtual GPU Driver\"\n    default y" >> $(KERNEL_DEST_DIR)/Kconfig; \
	fi
	@# Add entry to Makefile
	@if ! grep -q "CONFIG_CUSTOM_VGPU_DRV" $(KERNEL_DEST_DIR)/Makefile; then \
		echo "obj-\$$(CONFIG_CUSTOM_VGPU_DRV) += $(KERNEL_OBJ)" >> $(KERNEL_DEST_DIR)/Makefile; \
	fi

kbuild:
	@echo "Building Linux Kernel..."
	$(MAKE) -C $(KERNEL_PATH) -j$$(nproc)

kclean:
	@echo "Cleaning driver build artifacts only..."
	$(MAKE) -C $(KERNEL_PATH) M=$(KERNEL_DEST_DIR) clean
