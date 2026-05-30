# simdDispatch standalone build
#
# Usage:
#   make              → build build/libsimd_dispatch.a
#   make test         → build and run tests
#   make examples     → build example binaries
#   make clean        → remove build/
#
# SIMD kernels require SIMDe headers.  The Makefile looks for them at
# ../../inst/include (the location inside this R package tree) but you can
# override:
#   make SIMDE_DIR=/path/to/simde-parent-dir

CC      ?= cc
AR      ?= ar
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -pedantic
LDFLAGS ?=

KDIR    = kernels

# SIMDe include directory (parent of the "simde/" folder).
SIMDE_DIR ?= ../../inst/include
ifneq ($(wildcard $(SIMDE_DIR)/simde/x86/sse2.h),)
    HAVE_SIMDE    := 1
    SIMDE_CFLAGS  := -I$(SIMDE_DIR)
else
    HAVE_SIMDE    := 0
    SIMDE_CFLAGS  :=
endif

# --------------------------------------------------------------------------
# Backend detection: probe the compiler with a tiny source snippet.
# Each backend requires SIMDe and the matching -m flag.
# --------------------------------------------------------------------------
ifeq ($(HAVE_SIMDE),1)
    try_flag = $(shell printf '%s\n%s\n' '#include <simde/x86/sse2.h>' 'int main(void){return 0;}' | $(CC) $(CFLAGS) $(SIMDE_CFLAGS) $(1) -x c - -o /dev/null 2>/dev/null && echo 1 || echo 0)

    HAVE_SSE2   := $(call try_flag,-msse2)
    HAVE_SSE41  := $(call try_flag,-msse4.1)
    HAVE_AVX2   := $(call try_flag,-mavx2)
    HAVE_AVX512 := $(call try_flag,-mavx512f -mavx512bw -mavx512vl)
    HAVE_NEON   := $(shell printf '%s\n' 'int main(void){return 0;}' | $(CC) $(CFLAGS) -x c - -o /dev/null 2>/dev/null && $(CC) -dM -E - < /dev/null | grep -q '__ARM_NEON' && echo 1 || echo 0)
    HAVE_WASM   := $(shell printf '%s\n' 'int main(void){return 0;}' | $(CC) $(CFLAGS) -msimd128 -x c - -o /dev/null 2>/dev/null && echo 1 || echo 0)
else
    HAVE_SSE2   := 0
    HAVE_SSE41  := 0
    HAVE_AVX2   := 0
    HAVE_AVX512 := 0
    HAVE_NEON   := 0
    HAVE_WASM   := 0
endif

# Flags passed to simd_dispatch.c so it knows which backends were compiled.
HAVE_FLAGS := \
    -DSD_HAVE_SSE2=$(HAVE_SSE2) \
    -DSD_HAVE_SSE41=$(HAVE_SSE41) \
    -DSD_HAVE_AVX2=$(HAVE_AVX2) \
    -DSD_HAVE_AVX512=$(HAVE_AVX512) \
    -DSD_HAVE_NEON=$(HAVE_NEON) \
    -DSD_HAVE_WASM_SIMD128=$(HAVE_WASM)

# --------------------------------------------------------------------------
# Object file lists
# --------------------------------------------------------------------------
LIB_OBJS  = build/cpu_features.o build/simd_dispatch.o
KERN_OBJS = build/kernel_scalar.o

ifeq ($(HAVE_SSE2),1)
    KERN_OBJS += build/kernel_sse2.o
endif
ifeq ($(HAVE_SSE41),1)
    KERN_OBJS += build/kernel_sse41.o
endif
ifeq ($(HAVE_AVX2),1)
    KERN_OBJS += build/kernel_avx2.o
endif
ifeq ($(HAVE_AVX512),1)
    KERN_OBJS += build/kernel_avx512.o
endif
ifeq ($(HAVE_NEON),1)
    KERN_OBJS += build/kernel_neon.o
endif
ifeq ($(HAVE_WASM),1)
    KERN_OBJS += build/kernel_wasm_simd128.o
endif

ALL_OBJS = $(LIB_OBJS) $(KERN_OBJS)

# --------------------------------------------------------------------------
# Targets
# --------------------------------------------------------------------------
.PHONY: all test examples clean info

all: build/libsimd_dispatch.a

info:
	@echo "SIMDe found : $(HAVE_SIMDE)  ($(SIMDE_DIR))"
	@echo "SSE2        : $(HAVE_SSE2)"
	@echo "SSE4.1      : $(HAVE_SSE41)"
	@echo "AVX2        : $(HAVE_AVX2)"
	@echo "AVX-512     : $(HAVE_AVX512)"
	@echo "NEON        : $(HAVE_NEON)"
	@echo "WASM SIMD   : $(HAVE_WASM)"

build/:
	mkdir -p build

build/cpu_features.o: cpu_features.c cpu_features.h | build/
	$(CC) $(CFLAGS) -I$(KDIR) -c $< -o $@

build/simd_dispatch.o: simd_dispatch.c simd_dispatch.h cpu_features.h | build/
	$(CC) $(CFLAGS) -I$(KDIR) $(HAVE_FLAGS) -c $< -o $@

build/kernel_scalar.o: $(KDIR)/kernel_scalar.c $(KDIR)/kernel_api.h | build/
	$(CC) $(CFLAGS) -I$(KDIR) -c $< -o $@

build/kernel_sse2.o: $(KDIR)/kernel_sse2.c $(KDIR)/kernel_api.h | build/
	$(CC) $(CFLAGS) $(SIMDE_CFLAGS) -I$(KDIR) -msse2 -c $< -o $@

build/kernel_sse41.o: $(KDIR)/kernel_sse41.c $(KDIR)/kernel_api.h | build/
	$(CC) $(CFLAGS) $(SIMDE_CFLAGS) -I$(KDIR) -msse4.1 -c $< -o $@

build/kernel_avx2.o: $(KDIR)/kernel_avx2.c $(KDIR)/kernel_api.h | build/
	$(CC) $(CFLAGS) $(SIMDE_CFLAGS) -I$(KDIR) -mavx2 -c $< -o $@

build/kernel_avx512.o: $(KDIR)/kernel_avx512.c $(KDIR)/kernel_api.h | build/
	$(CC) $(CFLAGS) $(SIMDE_CFLAGS) -I$(KDIR) -mavx512f -mavx512bw -mavx512vl -c $< -o $@

build/kernel_neon.o: $(KDIR)/kernel_neon.c $(KDIR)/kernel_api.h | build/
	$(CC) $(CFLAGS) $(SIMDE_CFLAGS) -I$(KDIR) -c $< -o $@

build/kernel_wasm_simd128.o: $(KDIR)/kernel_wasm_simd128.c $(KDIR)/kernel_api.h | build/
	$(CC) $(CFLAGS) $(SIMDE_CFLAGS) -I$(KDIR) -msimd128 -c $< -o $@

build/libsimd_dispatch.a: $(ALL_OBJS)
	$(AR) rcs $@ $^

# --------------------------------------------------------------------------
# Test and examples
# --------------------------------------------------------------------------
test: build/test_dispatch
	./build/test_dispatch

build/test_dispatch: test/test_dispatch.c build/libsimd_dispatch.a | build/
	$(CC) $(CFLAGS) -I. -I$(KDIR) $< build/libsimd_dispatch.a -o $@ $(LDFLAGS)

examples: build/example_count

build/example_count: examples/example_count.c build/libsimd_dispatch.a | build/
	$(CC) $(CFLAGS) -I. -I$(KDIR) $< build/libsimd_dispatch.a -o $@ $(LDFLAGS)

clean:
	rm -rf build/
