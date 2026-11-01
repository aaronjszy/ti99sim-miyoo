# Build and package TI-99/Sim (Libretro core) for the Miyoo Mini (Onion OS).
# Only Docker (plus git and a shell) is needed on your machine; the cross-compiler lives in the toolchain image.

# Onion's cross-compile image, pinned by digest so builds are repeatable. Override to use a different one.
IMAGE  ?= aemiii91/miyoomini-toolchain@sha256:e5123590ad75d27f0f4c91196e3119a255cad45f3ae15243e29a8e0a2ec50132
DOCKER ?= docker

# Network deployment settings for Onion OS
# Supply the IP or hostname via:
#   1. Command line: make deploy IP=192.168.1.50
#   2. Local file:   echo "192.168.1.50" > .miyoo-ip
#   3. Default:      miyoo.local
MIYOO_USER ?= onion
MIYOO_DIR  ?= /mnt/SDCARD

# Read from .miyoo-ip if present
ifneq ($(wildcard .miyoo-ip),)
MIYOO_HOST_FILE := $(shell cat .miyoo-ip 2>/dev/null | tr -d '[:space:]')
endif

# Precedence: IP=... -> HOST=... -> .miyoo-ip file -> miyoo.local
ifdef IP
MIYOO_HOST := $(IP)
else ifdef HOST
MIYOO_HOST := $(HOST)
else ifdef MIYOO_HOST_FILE
MIYOO_HOST := $(MIYOO_HOST_FILE)
else
MIYOO_HOST ?= miyoo.local
endif

# If MIYOO_HOST contains an @ (e.g. onion@192.168.1.50), use it directly, otherwise prepend MIYOO_USER@
MIYOO_SSH_TARGET := $(if $(findstring @,$(MIYOO_HOST)),$(MIYOO_HOST),$(MIYOO_USER)@$(MIYOO_HOST))

.PHONY: help build build-host package zip install deploy scp patch clean distclean

help:
	@echo "make build                     cross-compile Libretro core in Docker -> build/ti99sim_libretro.so"
	@echo "make build-host                compile Libretro core natively on host -> ti99sim/bin/ti99sim_libretro.*"
	@echo "make package                   assemble dist/SDCARD, a tree mirroring the SD card"
	@echo "make zip                       package, then zip it -> dist/ti99sim-miyoo.zip"
	@echo "make install SDCARD=/path      package, then copy onto a mounted Onion SD card"
	@echo "make deploy [IP=...]           package, zip, scp to Miyoo and unzip in /mnt/SDCARD"
	@echo "                               (alias: 'make scp'; saves to .miyoo-ip to avoid typing IP each time)"
	@echo "make patch                     regenerate upstream-modifications.patch against commit e360854"
	@echo "make clean | distclean         remove build and dist artifacts"
	@echo
	@echo "The TI console ROM is not included; see README.md ('Required System ROMs')."

build:
	$(DOCKER) run --rm -u "$$(id -u):$$(id -g)" -v "$(CURDIR)":/work -w /work $(IMAGE) bash scripts/build-in-container.sh

build-host:
	CFLAGS="-DTI_NO_OPENSSL" $(MAKE) -C ti99sim/src/core
	CFLAGS="-DTI_NO_OPENSSL" $(MAKE) -C ti99sim/src/libretro

package: build
	@scripts/package.sh

zip: package
	@cd dist/SDCARD && rm -f ../ti99sim-miyoo.zip && zip -qr ../ti99sim-miyoo.zip . && echo "zip: dist/ti99sim-miyoo.zip"

install: package
	@test -n "$(SDCARD)" || { echo "usage: make install SDCARD=/path/to/mounted/sdcard"; exit 1; }
	@scripts/install.sh dist/SDCARD "$(SDCARD)"

deploy: zip
	@echo "Deploying dist/ti99sim-miyoo.zip to $(MIYOO_SSH_TARGET):$(MIYOO_DIR)..."
	scp dist/ti99sim-miyoo.zip $(MIYOO_SSH_TARGET):$(MIYOO_DIR)/ti99sim-miyoo.zip
	@echo "Unzipping on device..."
	ssh $(MIYOO_SSH_TARGET) "cd $(MIYOO_DIR) && unzip -oq ti99sim-miyoo.zip && rm -f ti99sim-miyoo.zip"
	@echo "Done! TI-99/Sim deployed to $(MIYOO_HOST)."

scp: deploy

patch:
	@scripts/update-patch.sh

clean:
	rm -rf build dist ti99sim/bin ti99sim/src/*/Release

distclean: clean

