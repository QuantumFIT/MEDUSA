# ==============================================================================
# MEDUSA Makefile
# ==============================================================================

SRC_DIR     := src
OBJ_DIR     := obj
BIN_DIR     := .
LIB_DIR     := lib
LACE_DIR    := $(LIB_DIR)/sylvan/build/_deps
BUDDY_DIR   := $(LIB_DIR)/MoToBuddy

# ==============================================================================
# Source and object file lists
# ==============================================================================

SRCS             := $(wildcard $(SRC_DIR)/*.c)
EXEC             := $(BIN_DIR)/MEDUSA
OBJS             := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o,           $(SRCS))
OBJS_BUDDY_GMP   := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/buddy_gmp/%.o, $(SRCS))

# ==============================================================================
# Interface / leaf subdirectories
# ==============================================================================

INTERFACE_DIR     := $(SRC_DIR)/interface_implementations
BACKENDS_DIR      := $(INTERFACE_DIR)/backends
LEAF_PRIM_DIR     := $(INTERFACE_DIR)/leaf_primitives
LEAF_ALG_DIR      := $(INTERFACE_DIR)/leaves
INC_DIR_INTERFACE := $(INTERFACE_DIR)/inc

# ==============================================================================
# Interface and leaf object paths
# ==============================================================================

INTERFACE_OBJ_default        := $(OBJ_DIR)/interface_sylvan_gmp.o
INTERFACE_OBJ_sylvan_gmp     := $(OBJ_DIR)/interface_sylvan_gmp.o
INTERFACE_OBJ_sylvan_doubles := $(OBJ_DIR)/interface_sylvan_doubles.o
INTERFACE_OBJ_motobuddy      := $(OBJ_DIR)/interface_motobuddy.o
LEAF_OBJ_mpz                 := $(OBJ_DIR)/leaf_primitive_mpz.o
LEAF_OBJ_algebraic           := $(OBJ_DIR)/leaf_algebraic_mpz.o

# ==============================================================================
# Compiler and base flags
# ==============================================================================

CC     := gcc
CXX    := g++
CFLAGS := -O2 -g
CLIBS  := -lgmp -lpthread -lm

# USE_CXX=1 compiles gates.c as C++ (enables mtbdd_traverse_to/mtbdd_swap path)
# and also compiles sim_mosf.cpp (MOSF simulation support).
# All other files are always compiled with gcc.
USE_CXX ?= 0

ifeq ($(USE_CXX), 1)
  GATES_CC      := $(CXX)
  GATES_FLAGS   := -x c++ -std=c++17 -fext-numeric-literals -g
  LINKER        := $(CXX)
  USE_MOSF_FLAG := -DUSE_MOSF
else
  GATES_CC      := $(CC)
  GATES_FLAGS   :=
  LINKER        := $(CC)
  USE_MOSF_FLAG :=
endif

# ==============================================================================
# Profiling — callgrind (valgrind)
# Usage: make buddy_doubles_f64 PROFILE=1
# Then:  valgrind --tool=callgrind ./MEDUSA_buddy_doubles_f64 ...
#        callgrind_annotate callgrind.out.<pid>
# ==============================================================================

PROFILE ?= 0

ifeq ($(PROFILE), 1)
  # Disable optimisation so callgrind sees real call graph;
  # keep -g so source annotations work.
  CFLAGS := -O0 -g
endif

# ==============================================================================
# Float type selection for buddy_doubles
# 0 = float   1 = double (default)   2 = long double   3 = __float128
# ==============================================================================

LEAF_FLOAT_TYPE ?= 1

ifeq ($(LEAF_FLOAT_TYPE), 0)
  FLOAT_SUFFIX := f32
else ifeq ($(LEAF_FLOAT_TYPE), 1)
  FLOAT_SUFFIX := f64
else ifeq ($(LEAF_FLOAT_TYPE), 2)
  FLOAT_SUFFIX := f80
else ifeq ($(LEAF_FLOAT_TYPE), 3)
  FLOAT_SUFFIX := f128
  CLIBS        += -lquadmath
else
  $(error Unknown LEAF_FLOAT_TYPE=$(LEAF_FLOAT_TYPE))
endif

# ==============================================================================
# buddy_doubles paths
# ==============================================================================

DOUBLES_OBJ_DIR    := $(OBJ_DIR)/buddy_doubles_$(FLOAT_SUFFIX)
OBJS_BUDDY_DOUBLES := $(patsubst $(SRC_DIR)/%.c, $(DOUBLES_OBJ_DIR)/%.o, $(SRCS))

INTERFACE_OBJ_buddy_doubles := $(DOUBLES_OBJ_DIR)/interface_motobuddy.o
LEAF_OBJ_double             := $(OBJ_DIR)/leaf_primitive_double_$(FLOAT_SUFFIX).o
LEAF_OBJ_reim               := $(OBJ_DIR)/leaf_reim_double_$(FLOAT_SUFFIX).o

# ==============================================================================
# buddy_mpfr paths and flags
# ==============================================================================

MPFR_PRECISION ?= 256

BUDDY_MPFR_OBJ_DIR := $(OBJ_DIR)/buddy_mpfr
OBJS_BUDDY_MPFR    := $(patsubst $(SRC_DIR)/%.c, $(BUDDY_MPFR_OBJ_DIR)/%.o, $(SRCS))

INTERFACE_OBJ_buddy_mpfr := $(BUDDY_MPFR_OBJ_DIR)/interface_motobuddy.o
LEAF_OBJ_mpfr            := $(OBJ_DIR)/leaf_primitive_mpfr.o
LEAF_OBJ_reim_mpfr       := $(OBJ_DIR)/leaf_reim_mpfr.o

# ==============================================================================
# Include paths
# MoToBuddy headers (bdd.h, mtbdd.h) must come before $(SRC_DIR) in all buddy
# backends to prevent src/mtbdd.h from shadowing lib/MoToBuddy/src/mtbdd.h.
# ==============================================================================

INC_DIRS := -I $(SRC_DIR)
INC_DIRS += -I $(INC_DIR_INTERFACE)
INC_DIRS += -I $(LIB_DIR)/sylvan/src/
INC_DIRS += -I $(LACE_DIR)/lace-src/src/
INC_DIRS += -I $(LACE_DIR)/lace-build/
INC_DIRS += -I $(BUDDY_DIR)/src/
INC_DIRS += -I $(BACKENDS_DIR) -I $(LEAF_PRIM_DIR) -I $(LEAF_ALG_DIR)

INC_DIRS_BUDDY_GMP := -I $(BUDDY_DIR)/src/
INC_DIRS_BUDDY_GMP += -I $(SRC_DIR)
INC_DIRS_BUDDY_GMP += -I $(INC_DIR_INTERFACE)
INC_DIRS_BUDDY_GMP += -I $(BACKENDS_DIR) -I $(LEAF_PRIM_DIR) -I $(LEAF_ALG_DIR)

INC_DIRS_BUDDY_DOUBLES := -I $(BUDDY_DIR)/src/
INC_DIRS_BUDDY_DOUBLES += -I $(BUDDY_DIR)/build/include
INC_DIRS_BUDDY_DOUBLES += -I $(SRC_DIR)
INC_DIRS_BUDDY_DOUBLES += -I $(INC_DIR_INTERFACE)
INC_DIRS_BUDDY_DOUBLES += -I $(BACKENDS_DIR) -I $(LEAF_PRIM_DIR) -I $(LEAF_ALG_DIR)

INC_DIRS_BUDDY_MPFR := -I $(BUDDY_DIR)/src/
INC_DIRS_BUDDY_MPFR += -I $(SRC_DIR)
INC_DIRS_BUDDY_MPFR += -I $(INC_DIR_INTERFACE)
INC_DIRS_BUDDY_MPFR += -I $(BACKENDS_DIR) -I $(LEAF_PRIM_DIR) -I $(LEAF_ALG_DIR)

# ==============================================================================
# Backend-specific compile flags
# ==============================================================================

BUDDY_GMP_CFLAGS := -DLEAF_BACKEND_GMP \
                    -include $(LEAF_PRIM_DIR)/leaf_primitive_mpz.h \
                    -include $(BACKENDS_DIR)/interface_motobuddy.h

BUDDY_DOUBLES_CFLAGS := -DLEAF_BACKEND_DOUBLES \
                        -DLEAF_FLOAT_TYPE=$(LEAF_FLOAT_TYPE) \
                        -include $(LEAF_PRIM_DIR)/leaf_primitive_double.h \
                        -include $(BACKENDS_DIR)/interface_motobuddy.h

BUDDY_MPFR_CFLAGS := -DLEAF_BACKEND_MPFR \
                     -DMPFR_PRECISION=$(MPFR_PRECISION) \
                     -include $(LEAF_PRIM_DIR)/leaf_primitive_mpfr.h \
                     -include $(BACKENDS_DIR)/interface_motobuddy.h

# ==============================================================================
# Misc config
# ==============================================================================

N_JOBS             := 4
OF_TYPE            := pdf
F_OUT_NAME         := res
LONG_NUMS_OUT_FILE := res-vars.txt
BSCRIPT_PATH       := benchmark-utils/scripts

# ==============================================================================
# Phony targets
# ==============================================================================

.DEFAULT_GOAL := all

.PHONY: all clean clean-all clean-artifacts clean-deps clean-benchmark     \
        plot benchmarks                                                     \
        init make-sylvan download-sylvan make-sliqsim                      \
        init-motobuddy make-motobuddy download-motobuddy                   \
        sylvan_gmp sylvan_doubles                                          \
        buddy_gmp buddy_mpfr                                               \
        buddy_doubles                                                      \
        buddy_doubles_f32 buddy_doubles_f64 buddy_doubles_f80              \
        buddy_doubles_f128 buddy_doubles_all

# ==============================================================================
# Default target (sylvan_gmp backend)
# ==============================================================================

all: $(OBJS) $(INTERFACE_OBJ_default) \
     $(LIB_DIR)/sylvan/build/src/lib/libsylvan.a \
     $(LACE_DIR)/lace-build/lib/liblace.a | $(BIN_DIR)
	$(LINKER) $(INC_DIRS) $(CFLAGS) -o $(EXEC) $^ $(CLIBS)

# ==============================================================================
# Sylvan targets
# ==============================================================================

sylvan_gmp: $(OBJS) $(INTERFACE_OBJ_sylvan_gmp) \
            $(LIB_DIR)/sylvan/build/src/lib/libsylvan.a \
            $(LACE_DIR)/lace-build/lib/liblace.a | $(BIN_DIR)
	$(CC) $(INC_DIRS) $(CFLAGS) -o $(BIN_DIR)/MEDUSA_sylvan_gmp $^ $(CLIBS)

sylvan_doubles: $(OBJS) $(INTERFACE_OBJ_sylvan_doubles) \
                $(LIB_DIR)/sylvan/build/src/lib/libsylvan.a \
                $(LACE_DIR)/lace-build/lib/liblace.a | $(BIN_DIR)
	$(CC) $(INC_DIRS) $(CFLAGS) -o $(BIN_DIR)/MEDUSA_sylvan_doubles $^ $(CLIBS)

# ==============================================================================
# buddy_gmp target
# ==============================================================================

buddy_gmp: $(OBJS_BUDDY_GMP) $(INTERFACE_OBJ_motobuddy) \
           $(LEAF_OBJ_mpz) $(LEAF_OBJ_algebraic) \
           $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a | $(BIN_DIR)
	$(LINKER) $(INC_DIRS_BUDDY_GMP) $(CFLAGS) -o $(BIN_DIR)/MEDUSA_buddy_gmp $^ $(CLIBS)

# ==============================================================================
# buddy_mpfr target
# ==============================================================================

buddy_mpfr: $(OBJS_BUDDY_MPFR) $(INTERFACE_OBJ_buddy_mpfr) \
            $(LEAF_OBJ_mpfr) $(LEAF_OBJ_reim_mpfr) \
            $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a | $(BIN_DIR)
	$(LINKER) $(INC_DIRS_BUDDY_MPFR) $(CFLAGS) \
	    -o $(BIN_DIR)/MEDUSA_buddy_mpfr_$(MPFR_PRECISION) $^ $(CLIBS) -lmpfr

# ==============================================================================
# buddy_doubles target (parameterized by LEAF_FLOAT_TYPE)
# ==============================================================================

ifeq ($(USE_CXX), 1)
SIM_MOSF_OBJ := $(DOUBLES_OBJ_DIR)/sim_mosf.o
else
SIM_MOSF_OBJ :=
endif

buddy_doubles: $(SIM_MOSF_OBJ) \
               $(OBJS_BUDDY_DOUBLES) \
               $(INTERFACE_OBJ_buddy_doubles) \
               $(LEAF_OBJ_double) \
               $(LEAF_OBJ_reim) \
               $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a | $(BIN_DIR)
	$(LINKER) $(INC_DIRS_BUDDY_DOUBLES) $(CFLAGS) \
	    -o $(BIN_DIR)/MEDUSA_buddy_doubles_$(FLOAT_SUFFIX) $^ $(CLIBS)

# Convenience aliases — recurse with the correct LEAF_FLOAT_TYPE
buddy_doubles_f32:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=0

buddy_doubles_f64:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=1

buddy_doubles_f80:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=2

buddy_doubles_f128:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=3

# Build all four float variants in sequence
buddy_doubles_all:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=0
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=1
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=2
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=3

# ==============================================================================
# Object rules — buddy_doubles
# gates.o and gates_symb.o use GATES_CC/GATES_FLAGS (g++ -x c++ when USE_CXX=1).
# main.o gets USE_MOSF_FLAG to conditionally expose sim_mosf_file.
# sim_mosf.o only built when USE_CXX=1.
# All other objects always use CC (gcc).
# ==============================================================================

$(DOUBLES_OBJ_DIR)/gates.o: $(SRC_DIR)/gates.c | $(DOUBLES_OBJ_DIR)
	$(GATES_CC) $(INC_DIRS_BUDDY_DOUBLES) $(CFLAGS) $(GATES_FLAGS) \
	    $(BUDDY_DOUBLES_CFLAGS) -MMD -MP -c $< -o $@

$(DOUBLES_OBJ_DIR)/gates_symb.o: $(SRC_DIR)/gates_symb.c | $(DOUBLES_OBJ_DIR)
	$(GATES_CC) $(INC_DIRS_BUDDY_DOUBLES) $(CFLAGS) $(GATES_FLAGS) \
	    $(BUDDY_DOUBLES_CFLAGS) -MMD -MP -c $< -o $@

$(DOUBLES_OBJ_DIR)/main.o: $(SRC_DIR)/main.c | $(DOUBLES_OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_DOUBLES) $(CFLAGS) $(BUDDY_DOUBLES_CFLAGS) \
	    $(USE_MOSF_FLAG) -MMD -MP -c $< -o $@

ifeq ($(USE_CXX), 1)
$(DOUBLES_OBJ_DIR)/sim_mosf.o: $(SRC_DIR)/sim_mosf.cpp | $(DOUBLES_OBJ_DIR)
	$(CXX) $(INC_DIRS_BUDDY_DOUBLES) $(CFLAGS) -std=c++17 -fext-numeric-literals \
	    $(BUDDY_DOUBLES_CFLAGS) -MMD -MP -c $< -o $@
endif

$(DOUBLES_OBJ_DIR)/interface_motobuddy.o: $(BACKENDS_DIR)/interface_motobuddy.c \
                                          | $(DOUBLES_OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_DOUBLES) $(CFLAGS) $(BUDDY_DOUBLES_CFLAGS) \
	    -MMD -MP -c $< -o $@

$(OBJ_DIR)/leaf_primitive_double_$(FLOAT_SUFFIX).o: \
        $(LEAF_PRIM_DIR)/leaf_primitive_double.c | $(OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_DOUBLES) $(CFLAGS) $(BUDDY_DOUBLES_CFLAGS) \
	    -MMD -MP -c $< -o $@

$(OBJ_DIR)/leaf_reim_double_$(FLOAT_SUFFIX).o: \
        $(LEAF_ALG_DIR)/leaf_reim_double.c | $(OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_DOUBLES) $(CFLAGS) $(BUDDY_DOUBLES_CFLAGS) \
	    -MMD -MP -c $< -o $@

$(DOUBLES_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(DOUBLES_OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_DOUBLES) $(CFLAGS) $(BUDDY_DOUBLES_CFLAGS) \
	    -MMD -MP -c $< -o $@

# ==============================================================================
# Object rules — buddy_gmp
# ==============================================================================

$(OBJ_DIR)/buddy_gmp/gates.o: $(SRC_DIR)/gates.c | $(OBJ_DIR)/buddy_gmp
	$(GATES_CC) $(INC_DIRS_BUDDY_GMP) $(CFLAGS) $(GATES_FLAGS) \
	    $(BUDDY_GMP_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/buddy_gmp/gates_symb.o: $(SRC_DIR)/gates_symb.c | $(OBJ_DIR)/buddy_gmp
	$(GATES_CC) $(INC_DIRS_BUDDY_GMP) $(CFLAGS) $(GATES_FLAGS) \
	    $(BUDDY_GMP_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/interface_motobuddy.o: $(BACKENDS_DIR)/interface_motobuddy.c | $(OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_GMP) $(CFLAGS) $(BUDDY_GMP_CFLAGS) \
	    -MMD -MP -c $< -o $@

$(OBJ_DIR)/leaf_primitive_mpz.o: $(LEAF_PRIM_DIR)/leaf_primitive_mpz.c | $(OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_GMP) $(CFLAGS) $(BUDDY_GMP_CFLAGS) \
	    -MMD -MP -c $< -o $@

$(OBJ_DIR)/leaf_algebraic_mpz.o: $(LEAF_ALG_DIR)/leaf_algebraic_mpz.c | $(OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_GMP) $(CFLAGS) $(BUDDY_GMP_CFLAGS) \
	    -MMD -MP -c $< -o $@

$(OBJ_DIR)/buddy_gmp/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)/buddy_gmp
	$(CC) $(INC_DIRS_BUDDY_GMP) $(CFLAGS) $(BUDDY_GMP_CFLAGS) \
	    -MMD -MP -c $< -o $@

# ==============================================================================
# Object rules — buddy_mpfr
# ==============================================================================

$(BUDDY_MPFR_OBJ_DIR)/gates.o: $(SRC_DIR)/gates.c | $(BUDDY_MPFR_OBJ_DIR)
	$(GATES_CC) $(INC_DIRS_BUDDY_MPFR) $(CFLAGS) $(GATES_FLAGS) \
	    $(BUDDY_MPFR_CFLAGS) -MMD -MP -c $< -o $@

$(BUDDY_MPFR_OBJ_DIR)/gates_symb.o: $(SRC_DIR)/gates_symb.c | $(BUDDY_MPFR_OBJ_DIR)
	$(GATES_CC) $(INC_DIRS_BUDDY_MPFR) $(CFLAGS) $(GATES_FLAGS) \
	    $(BUDDY_MPFR_CFLAGS) -MMD -MP -c $< -o $@

$(BUDDY_MPFR_OBJ_DIR)/interface_motobuddy.o: $(BACKENDS_DIR)/interface_motobuddy.c \
                                              | $(BUDDY_MPFR_OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_MPFR) $(CFLAGS) $(BUDDY_MPFR_CFLAGS) \
	    -MMD -MP -c $< -o $@

$(OBJ_DIR)/leaf_primitive_mpfr.o: $(LEAF_PRIM_DIR)/leaf_primitive_mpfr.c | $(OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_MPFR) $(CFLAGS) $(BUDDY_MPFR_CFLAGS) \
	    -MMD -MP -c $< -o $@

$(OBJ_DIR)/leaf_reim_mpfr.o: $(LEAF_ALG_DIR)/leaf_reim_double.c | $(OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_MPFR) $(CFLAGS) $(BUDDY_MPFR_CFLAGS) \
	    -MMD -MP -c $< -o $@

$(BUDDY_MPFR_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(BUDDY_MPFR_OBJ_DIR)
	$(CC) $(INC_DIRS_BUDDY_MPFR) $(CFLAGS) $(BUDDY_MPFR_CFLAGS) \
	    -MMD -MP -c $< -o $@

# ==============================================================================
# Object rules — sylvan (plain gcc, no backend flags)
# ==============================================================================

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c       | $(OBJ_DIR)
	$(CC) $(INC_DIRS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: $(INTERFACE_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(INC_DIRS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: $(BACKENDS_DIR)/%.c  | $(OBJ_DIR)
	$(CC) $(INC_DIRS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: $(LEAF_PRIM_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(INC_DIRS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: $(LEAF_ALG_DIR)/%.c  | $(OBJ_DIR)
	$(CC) $(INC_DIRS) $(CFLAGS) -MMD -MP -c $< -o $@

# ==============================================================================
# Directory creation
# ==============================================================================

$(BIN_DIR) $(OBJ_DIR) $(OBJ_DIR)/buddy_gmp $(DOUBLES_OBJ_DIR) $(BUDDY_MPFR_OBJ_DIR):
	mkdir -p $@

# ==============================================================================
# Dependency inclusion
# ==============================================================================

-include $(OBJS:.o=.d)
-include $(OBJS_BUDDY_GMP:.o=.d)
-include $(OBJS_BUDDY_DOUBLES:.o=.d)
-include $(OBJS_BUDDY_MPFR:.o=.d)

# ==============================================================================
# Utility targets
# ==============================================================================

plot:
	@dot -T$(OF_TYPE) $(F_OUT_NAME).dot -o $(F_OUT_NAME).$(OF_TYPE)

benchmarks:
	@bash ./$(BSCRIPT_PATH)/run-benchmarks.sh

# ==============================================================================
# Init / dependency targets
# ==============================================================================

init: make-sylvan
	mkdir -p $(LIB_DIR) && mv sylvan $(LIB_DIR)

make-sylvan: download-sylvan
	cd sylvan; mkdir build; cd build; cmake ..; make -j $(N_JOBS)

download-sylvan:
	@git clone https://github.com/trolando/sylvan.git || true

init-motobuddy: make-motobuddy
	mkdir -p $(LIB_DIR) && rm -rf $(LIB_DIR)/MoToBuddy && mv MoToBuddy $(LIB_DIR)

make-motobuddy: download-motobuddy
	cd MoToBuddy && \
	mkdir -p build && \
	cd build && \
	cmake .. -DCMAKE_CXX_STANDARD=17 \
	         -DCMAKE_BUILD_TYPE=$(if $(filter 1,$(PROFILE)),RelWithDebInfo,Release) && \
	make -j$(N_JOBS) buddy

download-motobuddy:
	@git clone https://github.com/VeriFIT/MoToBuddy.git || true

make-sliqsim:
	cd .. && \
	git clone https://github.com/NTU-ALComLab/SliQSim.git || true && \
	cd SliQSim/cudd && \
	./configure --enable-dddmp --enable-obj --enable-shared --enable-static && \
	cd .. && make

# ==============================================================================
# Clean targets
# ==============================================================================

clean: clean-artifacts

clean-all: clean-artifacts clean-deps clean-benchmark

clean-artifacts:
	rm -rf $(EXEC) $(F_OUT_NAME).dot $(F_OUT_NAME).$(OF_TYPE) \
	       $(LONG_NUMS_OUT_FILE) $(OBJ_DIR) \
	       MEDUSA_buddy_doubles_f32 MEDUSA_buddy_doubles_f64 \
	       MEDUSA_buddy_doubles_f80 MEDUSA_buddy_doubles_f128 \
	       MEDUSA_buddy_mpfr_256 MEDUSA_buddy_mpfr_512

clean-deps:
	rm -rf $(LIB_DIR)

clean-benchmark:
	cd .. && rm -rf SliQSim
