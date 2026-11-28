# SPDX-License-Identifier: GPL-3.0-or-later
CC ?= cc
CFLAGS ?= -O2
BASE = -std=c11 -Wall -Wextra -Werror -I Libraries/include
SRC = native/filesystems/vmfs6/vmfs6.c
HDR = native/filesystems/vmfs6/reader.h
.PHONY: all check clean
all: build/VMFS6.so build/freestanding/VMFS6.so
build/VMFS6.so: $(SRC) $(HDR) Makefile
	mkdir -p build
	$(CC) $(BASE) $(CFLAGS) -fPIC -fvisibility=hidden -shared -Wl,-z,defs -o $@ $(SRC)
build/freestanding/VMFS6.so: $(SRC) $(HDR) Makefile
	mkdir -p build/freestanding
	$(CC) $(BASE) -O2 -march=x86-64 -DIFS_FREESTANDING -ffreestanding -fno-builtin -fno-stack-protector -fno-tree-loop-distribute-patterns -fPIC -fvisibility=hidden -shared -nostdlib -Wl,-z,defs,-Bsymbolic -o $@ $(SRC)
build/test_vmfs6: tests/test_vmfs6.c
	mkdir -p build
	$(CC) $(BASE) $(CFLAGS) -o $@ $< -ldl
check: all build/test_vmfs6
	test "$$(nm -D --defined-only build/VMFS6.so | awk '{print $$3}')" = infiltrator_filesystem_api_v2
	test "$$(nm -D --defined-only build/freestanding/VMFS6.so | awk '{print $$3}')" = infiltrator_filesystem_api_v2
	test -z "$$(nm -D --undefined-only build/freestanding/VMFS6.so)"
	! readelf -d build/freestanding/VMFS6.so | grep -q NEEDED
	build/test_vmfs6 ./build/VMFS6.so
	build/test_vmfs6 ./build/freestanding/VMFS6.so
clean:
	rm -rf build

# Requires an independently captured partition and a payload copied from
# that same datastore by its original implementation. Never generate a
# synthetic volume or use the VMFS3 upstream test.img as a substitute.
.PHONY: check-real
check-real: check
	test -n "$(IMAGE)" && test -f "$(IMAGE)"
	test -n "$(PAYLOAD_PATH)" && test -f "$(EXPECTED)"
	test -n "$(IMAGE_SHA256)"
	printf '%s  %s\n' "$(IMAGE_SHA256)" "$(IMAGE)" | sha256sum -c -
	build/test_vmfs6 ./build/VMFS6.so "$(IMAGE)" "$(PAYLOAD_PATH)" "$(EXPECTED)"
	build/test_vmfs6 ./build/freestanding/VMFS6.so "$(IMAGE)" "$(PAYLOAD_PATH)" "$(EXPECTED)"
	printf '%s  %s\n' "$(IMAGE_SHA256)" "$(IMAGE)" | sha256sum -c -
