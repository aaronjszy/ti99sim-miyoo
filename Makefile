# Build and package TI-99/Sim for the Miyoo Mini (Onion OS).
# Only Docker (plus git and a shell) is needed on your machine; the cross-compiler lives in the toolchain image.

# Onion's cross-compile image, pinned by digest so builds are repeatable. Override to use a different one.
IMAGE  ?= aemiii91/miyoomini-toolchain@sha256:e5123590ad75d27f0f4c91196e3119a255cad45f3ae15243e29a8e0a2ec50132
DOCKER ?= docker

.PHONY: help deps build package zip install clean distclean

help:
	@echo "make deps                      download the pinned SDL2 port headers and libraries (deps/)"
	@echo "make build                     cross-compile the emulator in Docker -> build/ti99sim-sdl"
	@echo "make package                   assemble dist/SDCARD, a tree mirroring the SD card"
	@echo "make zip                       package, then zip it -> dist/ti99sim-miyoo.zip"
	@echo "make install SDCARD=/path      package, then copy onto a mounted Onion SD card"
	@echo "make clean | distclean         remove build output | also the downloaded deps"
	@echo
	@echo "The TI console ROM is not included; see README.md ('TI ROMs')."

deps:
	@scripts/fetch-deps.sh

build: deps
	$(DOCKER) run --rm -u "$$(id -u):$$(id -g)" -v "$(CURDIR)":/work -w /work $(IMAGE) bash scripts/build-in-container.sh

package: build
	@scripts/package.sh

zip: package
	@cd dist && rm -f ti99sim-miyoo.zip && zip -qr ti99sim-miyoo.zip SDCARD && echo "zip: dist/ti99sim-miyoo.zip"

install: package
	@test -n "$(SDCARD)" || { echo "usage: make install SDCARD=/path/to/mounted/sdcard"; exit 1; }
	@scripts/install.sh dist/SDCARD "$(SDCARD)"

clean:
	rm -rf build dist ti99sim/bin ti99sim/src/*/Release

distclean: clean
	rm -rf deps/steward-sdl2 deps/lib
