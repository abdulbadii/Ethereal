
#	Ethereal is a UCI chess playing engine authored by Andrew Grant.
#	<https://github.com/AndyGrant/Ethereal>     <andrew@grantnet.us>
#
#	Ethereal is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#
#	Ethereal is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <http://www.gnu.org/licenses/>.

### ==========================================================================
### Section 1. General Configuration
### ==========================================================================

### Executable name
EXE := Ethereal

### Installation dir definitions
PREFIX := /usr/local
BINDIR := $(PREFIX)/bin

### Object files
OBJS := attacks.o bitboards.o board.o evaluate.o history.o masks.o move.o movegen.o movepicker.o search.o syzygy.o texel.o thread.o time.o transposition.o uci.o windows.o zobrist.o fathom/tbprobe.o


### Establish the operating system name
KERNEL := $(shell uname -s)
ifeq ($(KERNEL),Linux)
	OS := $(shell uname -o)
endif

### ==========================================================================
### Section 2. High-level Configuration
### ==========================================================================
#
# flag                --- Comp switch --- Description
# ----------------------------------------------------------------------------
#
# debug := yes/no      --- -DNDEBUG         --- Enable/Disable debug mode
# sanitize := undefined/thread/no (-fsanitize )
#                     --- ( undefined )    --- enable undefined behavior checks
#                     --- ( thread    )    --- enable threading error  checks
# optimize := yes/no   --- (-O3/-fast etc.) --- Enable/Disable optimizations
# arch := (name)       --- (-arch)          --- Target architecture
# bits := 64/32        --- -DIS_64BIT       --- 64-/32-bit operating system
# prefetch := yes/no   --- -DUSE_PREFETCH   --- Use prefetch asm-instruction
# popcnt := yes/no     --- -DUSE_POPCNT     --- Use popcnt asm-instruction
# sse := yes/no        --- -msse            --- Use Intel Streaming SIMD Extensions
# pext := yes/no       --- -DUSE_PEXT       --- Use pext x86_64 asm-instruction
#
# Note that Makefile is space sensitive, so when adding new architectures
# or modifying existing flags, you have to make sure there are no extra spaces
# at the end of the line for flag values.

### 2.1. General and architecture defaults
optimize := yes
debug := no
sanitize := no
bits := 32
prefetch := no
popcnt := no
sse := no
pext := no

### 2.2 Architecture specific

ifeq ($(ARCH),general-32)
	arch := any
endif

ifeq ($(ARCH),x86-32-old)
	arch := i386
endif

ifeq ($(ARCH),x86-32)
	arch := i386
	prefetch := yes
	sse := yes
endif

ifeq ($(ARCH),general-64)
	arch := any
	bits := 64
endif

ifeq ($(ARCH),x86-64)
	arch := x86_64
	bits := 64
	prefetch := yes
	sse := yes
endif

ifeq ($(ARCH),x86-64-modern)
	arch := x86_64
	bits := 64
	prefetch := yes
	popcnt := yes
	sse := yes
endif

ifeq ($(ARCH),x86-64-bmi2)
	arch := x86_64
	bits := 64
	prefetch := yes
	popcnt := yes
	sse := yes
	pext := yes
endif

ifeq ($(ARCH),armv7)
	arch := armv7
	prefetch := yes
endif

ifeq ($(ARCH),ppc-32)
	arch := ppc
endif

ifeq ($(ARCH),ppc-64)
	arch := ppc64
	bits := 64
	popcnt := yes
	prefetch := yes
endif


### ==========================================================================
### Section 3. Low-level configuration
### ==========================================================================

### 3.1 Selecting compiler (default := gcc)

# -Wcast-qual -fno-exceptions
CFLAGS += -Wall -std=gnu11 -march=native -lpthread -lm $(EXTRACFLAGS)
DEPENDFLAGS += -std=gnu11
LDFLAGS += $(EXTRALDFLAGS)

ifneq ($(filter $(MODE),TEX TEXEL tex texel),)
	CFLAGS += -fopenmp -DTUNE
endif
ifneq ($(filter $(MODE),PRO PROFILE pro profile),)
	CFLAGS += -p -pg
endif
ifneq ($(filter $(MODE),REL RELEASE rel release),)
	LDFLAGS += -static
endif

ifneq ($(filter $(TYPE),TEX TEXEL tex texel),)
	CFLAGS += -fopenmp -DTUNE
endif
ifneq ($(filter $(TYPE),PRO PROFILE pro profile),)
	CFLAGS += -p -pg
endif
ifneq ($(filter $(TYPE),REL RELEASE rel release),)
	LDFLAGS += -static
endif

ifeq ($(COMP),)
	COMP := gcc
endif

ifeq ($(COMP),gcc)
	comp := gcc
	CC := gcc
	CFLAGS += -Wextra -Wshadow

	ifeq ($(ARCH),armv7)
		ifeq ($(OS),Android)
			CFLAGS += -m$(bits)
			LDFLAGS += -m$(bits)
		endif
	else
		CFLAGS += -m$(bits)
		LDFLAGS += -m$(bits)
	endif

	ifneq ($(KERNEL),Darwin)
	   LDFLAGS += -Wl,--no-as-needed
	endif
endif

ifeq ($(COMP),mingw)
	comp := mingw
	EXE:=$(EXE).exe

	ifeq ($(KERNEL),Linux)
		ifeq ($(bits),64)
			ifeq ($(shell which x86_64-w64-mingw32-c++-posix),)
				CC := x86_64-w64-mingw32-c++
			else
				CC := x86_64-w64-mingw32-c++-posix
			endif
		else
			ifeq ($(shell which i686-w64-mingw32-c++-posix),)
				CC := i686-w64-mingw32-c++
			else
				CC := i686-w64-mingw32-c++-posix
			endif
		endif
	else
		CC := gcc
	endif

	CFLAGS += -Wextra -Wshadow
# LDFLAGS += -static
endif

ifeq ($(COMP),icc)
	comp := icc
	CC := icpc
	CFLAGS += -diag-disable 1476,10120 -Wcheck -Wabi -Wdeprecated -strict-ansi
endif

ifeq ($(COMP),clang)
	comp := clang
	CC := clang++
	CFLAGS += -Wextra -Wshadow

	ifneq ($(KERNEL),Darwin)
	ifneq ($(KERNEL),OpenBSD)
		LDFLAGS += -latomic
	endif
	endif

	ifeq ($(ARCH),armv7)
		ifeq ($(OS),Android)
			CFLAGS += -m$(bits)
			LDFLAGS += -m$(bits)
		endif
	else
		CFLAGS += -m$(bits)
		LDFLAGS += -m$(bits)
	endif
endif

ifeq ($(comp),icc)
	profile_make := icc-profile-make
	profile_use := icc-profile-use
else
ifeq ($(comp),clang)
	profile_make := clang-profile-make
	profile_use := clang-profile-use
else
	profile_make := gcc-profile-make
	profile_use := gcc-profile-use
endif
endif

ifeq ($(KERNEL),Darwin)
	CFLAGS += -arch $(arch) -mmacosx-version-min=10.9
	LDFLAGS += -arch $(arch) -mmacosx-version-min=10.9
endif

### Travis CI script uses COMPILER to overwrite CC
ifdef COMPILER
	COMPCXX := $(COMPILER)
endif

### Allow overwriting CC from command line
ifdef COMPCXX
	CC := $(COMPCXX)
endif

### On mingw use Windows threads, otherwise POSIX
ifneq ($(comp),mingw)
	# On Android Bionic's C library comes with its own pthread implementation bundled in
	ifneq ($(OS),Android)
		# Haiku has pthreads in its libroot, so only link it in on other platforms
		ifneq ($(KERNEL),Haiku)
			LDFLAGS += -lpthread
		endif
	endif
endif

### 3.2.1 Debugging
ifeq ($(debug),no)
	CFLAGS += -DNDEBUG
else
	CFLAGS += -g
endif

### 3.2.2 Debugging with undefined behavior sanitizers
ifneq ($(sanitize),no)
        CFLAGS += -g3 -fsanitize=$(sanitize) -fuse-ld=gold
        LDFLAGS += -fsanitize=$(sanitize) -fuse-ld=gold
endif

### 3.3 Optimization
ifeq ($(optimize),yes)

	CFLAGS += -O3
	ifeq ($(comp),gcc)
		ifeq ($(OS), Android)
			CFLAGS += -fno-gcse -mthumb -march=armv7-a -mfloat-abi=softfp
		endif
	endif

	ifeq ($(comp),$(filter $(comp),gcc clang icc))
		ifeq ($(KERNEL),Darwin)
			CFLAGS += -mdynamic-no-pic
		endif
	endif
endif

### 3.4 Bits
ifeq ($(bits),64)
	CFLAGS += -DIS_64BIT
endif

### 3.5 prefetch
ifeq ($(prefetch),yes)
	ifeq ($(sse),yes)
		CFLAGS += -msse
		DEPENDFLAGS += -msse
	endif
else
	CFLAGS += -DNO_PREFETCH
endif

### 3.6 popcnt
ifeq ($(popcnt),yes)
	ifeq ($(arch),ppc64)
		CFLAGS += -DUSE_POPCNT
	else ifeq ($(comp),icc)
		CFLAGS += -msse3 -DUSE_POPCNT
	else
		CFLAGS += -msse3 -mpopcnt -DUSE_POPCNT
	endif
endif

### 3.7 pext
ifeq ($(pext),yes)
	CFLAGS += -DUSE_PEXT
	ifeq ($(comp),$(filter $(comp),gcc clang mingw))
		CFLAGS += -msse4 -mbmi2
	endif
endif

### 3.8 Link Time Optimization, it works since gcc 4.5 but not on mingw under Windows.
### This is a mix of compile and link time options because the lto link phase
### needs access to the optimization flags.
ifeq ($(optimize),yes)
ifeq ($(debug), no)
	ifeq ($(comp),$(filter $(comp),gcc clang))
		CFLAGS += -flto
		LDFLAGS += $(CFLAGS)
	endif

	ifeq ($(comp),mingw)
	CFLAGS += -flto
	ifeq ($(KERNEL),Linux)
		LDFLAGS += $(CFLAGS)
	endif
	endif
endif
endif

### 3.9 Android 5 can only run position independent executables. Note that this
### breaks Android 4.0 and earlier.
ifeq ($(OS), Android)
	CFLAGS += -fPIE
	LDFLAGS += -fPIE -pie
endif

### Built-in benchmark for pgo-builds
PGOBENCH := ./$(EXE) bench

### ==========================================================================
### Section 4. Public targets
### ==========================================================================

help:
	@echo ""
	@echo "To compile Ethereal, type: "
	@echo ""
	@echo "make target ARCH := arch [COMP := compiler] [COMPCXX := cxx]"
	@echo ""
	@echo "Supported targets:"
	@echo ""
	@echo "build                   > Standard build"
	@echo "profile-build           > PGO build"
	@echo "strip                   > Strip executable"
	@echo "install                 > Install executable"
	@echo "clean                   > Clean up"
	@echo ""
	@echo "Supported archs:"
	@echo ""
	@echo "x86-64-bmi2             > x86 64-bit with pext support (also enables SSE4)"
	@echo "x86-64-modern           > x86 64-bit with popcnt support (also enables SSE3)"
	@echo "x86-64                  > x86 64-bit generic"
	@echo "x86-32                  > x86 32-bit (also enables SSE)"
	@echo "x86-32-old              > x86 32-bit fall back for old hardware"
	@echo "ppc-64                  > PPC 64-bit"
	@echo "ppc-32                  > PPC 32-bit"
	@echo "armv7                   > ARMv7 32-bit"
	@echo "general-64              > unspecified 64-bit"
	@echo "general-32              > unspecified 32-bit"
	@echo ""
	@echo "Supported compilers:"
	@echo ""
	@echo "gcc                     > Gnu compiler (default)"
	@echo "mingw                   > Gnu compiler with MinGW under Windows"
	@echo "clang                   > LLVM Clang compiler"
	@echo "icc                     > Intel compiler"
	@echo ""
	@echo "Simple examples. If you don't know what to do, you likely want to run: "
	@echo ""
	@echo "make build ARCH := x86-64    (This is for 64-bit systems)"
	@echo "make build ARCH := x86-32    (This is for 32-bit systems)"
	@echo ""
	@echo "Advanced examples, for experienced users: "
	@echo ""
	@echo "make build ARCH := x86-64 COMP := clang"
	@echo "make profile-build ARCH := x86-64-bmi2 COMP := gcc COMPCXX := g++-4.8"
	@echo ""


.PHONY: help build profile-build strip install clean objclean profileclean help \
        config-sanity icc-profile-use icc-profile-make gcc-profile-use gcc-profile-make \
        clang-profile-use clang-profile-make

build: config-sanity
	$(MAKE) all

profile-build: config-sanity objclean profileclean
	@echo ""
	@echo "Step 1/4. Building instrumented executable ..."
	$(MAKE) $(profile_make)
	@echo ""
	@echo "Step 2/4. Running benchmark for pgo-build ..."
	$(PGOBENCH) > /dev/null
	@echo ""
	@echo "Step 3/4. Building optimized executable ..."
	$(MAKE) objclean
	$(MAKE) $(profile_use)
	@echo ""
	@echo "Step 4/4. Deleting profile data ..."
	$(MAKE) profileclean

strip:
	strip $(EXE)

install:
	-mkdir -p -m 755 $(BINDIR)
	-cp $(EXE) $(BINDIR)
	-strip $(BINDIR)/$(EXE)

#clean all
clean: objclean profileclean
	@rm -f .depend *~ core

# clean binaries and objects
objclean:
	@rm -f $(EXE) *.o fathom/*.o

# clean auxiliary profiling files
profileclean:
	@rm -rf profdir
	@rm -f bench.txt *.gcda fathom/*.gcda *.gcno fathom/*.gcno
	@rm -f Ethereal.profdata *.profraw

default:
	help

### ==========================================================================
### Section 5. Private targets
### ==========================================================================

all: $(EXE) .depend

config-sanity:
	@echo ""
	@echo "Config:"
	@echo "debug: '$(debug)'"
	@echo "sanitize: '$(sanitize)'"
	@echo "optimize: '$(optimize)'"
	@echo "arch: '$(arch)'"
	@echo "bits: '$(bits)'"
	@echo "kernel: '$(KERNEL)'"
	@echo "os: '$(OS)'"
	@echo "prefetch: '$(prefetch)'"
	@echo "popcnt: '$(popcnt)'"
	@echo "sse: '$(sse)'"
	@echo "pext: '$(pext)'"
	@echo ""
	@echo "Flags:"
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo ""
	@echo "Testing config sanity. If this fails, try 'make help' ..."
	@echo ""
	@test "$(debug)" = "yes" || test "$(debug)" = "no"
	@test "$(sanitize)" = "undefined" || test "$(sanitize)" = "thread" || test "$(sanitize)" = "address" || test "$(sanitize)" = "no"
	@test "$(optimize)" = "yes" || test "$(optimize)" = "no"
	@test "$(arch)" = "any" || test "$(arch)" = "x86_64" || test "$(arch)" = "i386" || \
	 test "$(arch)" = "ppc64" || test "$(arch)" = "ppc" || test "$(arch)" = "armv7"
	@test "$(bits)" = "32" || test "$(bits)" = "64"
	@test "$(prefetch)" = "yes" || test "$(prefetch)" = "no"
	@test "$(popcnt)" = "yes" || test "$(popcnt)" = "no"
	@test "$(sse)" = "yes" || test "$(sse)" = "no"
	@test "$(pext)" = "yes" || test "$(pext)" = "no"
	@test "$(comp)" = "gcc" || test "$(comp)" = "icc" || test "$(comp)" = "mingw" || test "$(comp)" = "clang"

$(EXE): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clang-profile-make:
	$(MAKE) \
	EXTRACFLAGS := '-fprofile-instr-generate ' \
	EXTRALDFLAGS := ' -fprofile-instr-generate' \
	all

clang-profile-use:
	llvm-profdata merge -output=Ethereal.profdata *.profraw
	$(MAKE) \
	EXTRACFLAGS := '-fprofile-instr-use=Ethereal.profdata' \
	EXTRALDFLAGS := '-fprofile-use ' \
	all

gcc-profile-make:
	$(MAKE) \
	EXTRACFLAGS := '-fprofile-generate' \
	EXTRALDFLAGS := '-lgcov' \
	all

gcc-profile-use:
	$(MAKE) \
	EXTRACFLAGS := '-fprofile-use -fno-peel-loops -fno-tracer' \
	EXTRALDFLAGS := '-lgcov' \
	all

icc-profile-make:
	@mkdir -p profdir
	$(MAKE) \
	EXTRACFLAGS := '-prof-gen=srcpos -prof_dir ./profdir' \
	all

icc-profile-use:
	$(MAKE) \
	EXTRACFLAGS := '-prof_use -prof_dir ./profdir' \
	all

.depend:
	-@$(CC) $(DEPENDFLAGS) -MM $(OBJS:.o=.c)> $@ 2> /dev/null

-include .depend