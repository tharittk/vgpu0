QEMU_PATH = ../env/qemu-11.0.0
BUILD_DIR = $(QEMU_PATH)/build
SRC_FILE = pci-vgpu0.c
DEST_DIR = $(QEMU_PATH)/hw/misc

.PHONY: all copy patch build clean

all: copy patch build

copy:
	@echo "Copying source..."
	cp $(SRC_FILE) $(DEST_DIR)/

patch:
	@echo "Patching Kconfig and meson.build..."
	@# Add to Kconfig if not present
	@if ! grep -q "config CUSTOM_PCI_DEV" $(DEST_DIR)/Kconfig; then \
		echo "\nconfig CUSTOM_PCI_DEV\n    bool\n    default y\n    depends on PCI" >> $(DEST_DIR)/Kconfig; \
	fi
	@# Add to meson.build if not present
	@if ! grep -q "CONFIG_CUSTOM_PCI_DEV" $(DEST_DIR)/meson.build; then \
		echo "system_ss.add(when: 'CONFIG_CUSTOM_PCI_DEV', if_true: files('$(SRC_FILE)'))" >> $(DEST_DIR)/meson.build; \
	fi

build:
	@echo "Building QEMU..."
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && meson .. && ninja

clean:
	rm -rf $(BUILD_DIR)
