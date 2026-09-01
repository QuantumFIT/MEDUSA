# ==============================================================================
# MEDUSA Makefile (MoToBuddy backends)
# ==============================================================================

SRC_DIR     := src
OBJ_DIR     := obj
BIN_DIR     := .
LIB_DIR     := lib
BUDDY_DIR   := $(LIB_DIR)/MoToBuddy

# ==============================================================================
# Source and object file lists
# ==============================================================================

MEDUSA_DEBUG ?= 0

SRCS             := $(wildcard $(SRC_DIR)/*.c)
# medusa_debug.c is only needed when MEDUSA_DEBUG=1 (header provides inlines otherwise).
ifneq ($(MEDUSA_DEBUG),1)
SRCS             := $(filter-out $(SRC_DIR)/medusa_debug.c,$(SRCS))
endif
EXEC             := $(BIN_DIR)/MEDUSA
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
# Medusa debug tracing (GC / protect / symb flakes)
# Usage: make buddy_doubles_f128 MEDUSA_DEBUG=1
# Runtime: MEDUSA_DEBUG=gc,symb,norm MEDUSA_DEBUG_FILE=/tmp/medusa.jsonl
# See src/medusa_debug.h
# ==============================================================================

ifeq ($(MEDUSA_DEBUG), 1)
  CFLAGS += -DMEDUSA_DEBUG
endif

PROFILE ?= 0

ifeq ($(PROFILE), 1)
  # Disable optimisation so callgrind sees real call graph;
  # keep -g so source annotations work.
  CFLAGS := -O0 -g
endif

# ==============================================================================
# Float type selection for buddy_doubles
# 0 = float   1 = double   2 = long double   3 = __float128 (default)
# ==============================================================================

LEAF_FLOAT_TYPE ?= 3

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

# libbuddy.a always includes mtbddop.cxx. C-linked buddy binaries still need
# the C++ runtime (operator new/delete, std::function, __gxx_personality_v0).
CLIBS += -lstdc++

# ==============================================================================
# buddy_doubles paths
# ==============================================================================

DOUBLES_OBJ_DIR    := $(OBJ_DIR)/buddy_doubles_$(FLOAT_SUFFIX)
OBJS_BUDDY_DOUBLES := $(patsubst $(SRC_DIR)/%.c, $(DOUBLES_OBJ_DIR)/%.o, $(SRCS))

INTERFACE_OBJ_buddy_doubles := $(DOUBLES_OBJ_DIR)/interface_motobuddy.o
LEAF_OBJ_double             := $(OBJ_DIR)/leaf_primitive_double_$(FLOAT_SUFFIX).o
LEAF_OBJ_reim               := $(OBJ_DIR)/leaf_reim_double_$(FLOAT_SUFFIX).o

# ==============================================================================
# Include paths
# MoToBuddy headers (bdd.h, mtbdd.h) must come before $(SRC_DIR) in all buddy
# backends to prevent src/mtbdd.h from shadowing lib/MoToBuddy/src/mtbdd.h.
# ==============================================================================

INC_DIRS_BUDDY_GMP := -I $(BUDDY_DIR)/src/
INC_DIRS_BUDDY_GMP += -I $(SRC_DIR)
INC_DIRS_BUDDY_GMP += -I $(INC_DIR_INTERFACE)
INC_DIRS_BUDDY_GMP += -I $(BACKENDS_DIR) -I $(LEAF_PRIM_DIR) -I $(LEAF_ALG_DIR)

INC_DIRS_BUDDY_DOUBLES := -I $(BUDDY_DIR)/src/
INC_DIRS_BUDDY_DOUBLES += -I $(BUDDY_DIR)/build/include
INC_DIRS_BUDDY_DOUBLES += -I $(SRC_DIR)
INC_DIRS_BUDDY_DOUBLES += -I $(INC_DIR_INTERFACE)
INC_DIRS_BUDDY_DOUBLES += -I $(BACKENDS_DIR) -I $(LEAF_PRIM_DIR) -I $(LEAF_ALG_DIR)

# ==============================================================================
# Backend-specific compile flags
# ==============================================================================

BUDDY_GMP_CFLAGS := -DLEAF_BACKEND_GMP \
                    -include $(LEAF_PRIM_DIR)/leaf_primitive_mpz.h \
                    -include $(BACKENDS_DIR)/interface_motobuddy.h

LEAF_ABS_EPS ?=
LEAF_REL_EPS ?=

BUDDY_DOUBLES_CFLAGS := -DLEAF_BACKEND_DOUBLES \
                        -DLEAF_FLOAT_TYPE=$(LEAF_FLOAT_TYPE) \
                        $(if $(LEAF_ABS_EPS),-DLEAF_ABS_EPS=$(LEAF_ABS_EPS)) \
                        $(if $(LEAF_REL_EPS),-DLEAF_REL_EPS=$(LEAF_REL_EPS)) \
                        -include $(LEAF_PRIM_DIR)/leaf_primitive_double.h \
                        -include $(BACKENDS_DIR)/interface_motobuddy.h

# ==============================================================================
# Misc config
# ==============================================================================

N_JOBS             := $(shell nproc 2>/dev/null || echo 4)
OF_TYPE            := pdf
F_OUT_NAME         := res
LONG_NUMS_OUT_FILE := res-vars.txt
BSCRIPT_PATH       := benchmark-utils/scripts

# ==============================================================================
# Phony targets
# ==============================================================================

.DEFAULT_GOAL := all

.PHONY: all help clean clean-all clean-artifacts clean-deps clean-benchmark \
        plot benchmarks                                                     \
        init init-motobuddy make-motobuddy download-motobuddy               \
        make-sliqsim                                                        \
        sylvan_gmp sylvan_doubles buddy_mpfr                                \
        buddy_gmp                                                           \
        buddy_doubles                                                      \
        buddy_doubles_f32 buddy_doubles_f64 buddy_doubles_f80              \
        buddy_doubles_f128 buddy_doubles_all                               \
        test test-unit test-circuits test-benchmarks test-metamorphic      \
        test-stress test-stress-f64 test-stress-f128 test-stress-gmp       \
        test-leaks                                                         \
        test-grover test-grover-all test-grover-f32 test-grover-f64        \
        test-grover-f80 test-grover-f128 test-grover-gmp                   \
        test-mutation

# ==============================================================================
# Default: MoToBuddy doubles (f128)
# ==============================================================================

all: buddy_doubles_f128

help:
	@echo "MEDUSA (MoToBuddy backends)"
	@echo "  make                  -> buddy_doubles_f128  (./MEDUSA -> ./MEDUSA_buddy_doubles_f128)"
	@echo "  make buddy_gmp        algebraic GMP leaves"
	@echo "  make buddy_doubles_f32|f64|f80|f128|all"
	@echo "  make USE_CXX=1 ...    C++ tree gates + MOSF (experimental)"
	@echo "  make init             clone and build lib/MoToBuddy"
	@echo "  make test             unit + circuits + benchmarks + metamorphic"
	@echo "Sylvan and buddy_mpfr targets are not available in this tree."

# Retired targets: sources are not in this repository.
sylvan_gmp sylvan_doubles:
	@echo >&2 "error: Sylvan backend sources are not in this tree."
	@echo >&2 "       Use:  make buddy_gmp   or   make buddy_doubles_f128"
	@false

buddy_mpfr:
	@echo >&2 "error: buddy_mpfr is not implemented (leaf_primitive_mpfr is missing)."
	@echo >&2 "       Use:  make buddy_doubles_f128  or  make buddy_gmp"
	@false

# ==============================================================================
# Tests (MoToBuddy)
# ==============================================================================

TEST_DIR          := tests
TEST_HARNESS_H    := $(TEST_DIR)/test_harness.h
TEST_UNIT_BIN     := $(BIN_DIR)/test_unit_api
TEST_UNIT_SRC     := $(TEST_DIR)/test_unit_api.c
# Link all buddy_doubles objects except main.o
TEST_UNIT_OBJS    := $(filter-out $(DOUBLES_OBJ_DIR)/main.o, $(OBJS_BUDDY_DOUBLES)) \
                     $(INTERFACE_OBJ_buddy_doubles) $(LEAF_OBJ_double) $(LEAF_OBJ_reim)

TEST_SEM_BIN      := $(BIN_DIR)/test_benchmark_semantics
TEST_SEM_SRC      := $(TEST_DIR)/test_benchmark_semantics.c

TEST_META_BIN     := $(BIN_DIR)/test_metamorphic
TEST_META_SRC     := $(TEST_DIR)/test_metamorphic.c

STRESS_LEVEL ?= 2
TEST_STRESS_SRC      := $(TEST_DIR)/test_stress.c
TEST_STRESS_DOUBLES_BIN := $(BIN_DIR)/test_stress_$(FLOAT_SUFFIX)
TEST_STRESS_F64_BIN  := $(BIN_DIR)/test_stress_f64
TEST_STRESS_F128_BIN := $(BIN_DIR)/test_stress_f128
TEST_STRESS_GMP_BIN  := $(BIN_DIR)/test_stress_gmp

TEST_STRESS_GMP_OBJS := $(filter-out $(OBJ_DIR)/buddy_gmp/main.o, $(OBJS_BUDDY_GMP)) \
                        $(INTERFACE_OBJ_motobuddy) $(LEAF_OBJ_mpz) $(LEAF_OBJ_algebraic)

test: test-unit test-circuits test-benchmarks test-metamorphic

test-unit:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=3
	$(MAKE) $(TEST_UNIT_BIN) LEAF_FLOAT_TYPE=3
	$(TEST_UNIT_BIN)

$(TEST_UNIT_BIN): $(TEST_UNIT_SRC) $(TEST_HARNESS_H) $(TEST_UNIT_OBJS) \
                  $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a
	$(CC) $(INC_DIRS_BUDDY_DOUBLES) -I $(TEST_DIR) $(CFLAGS) \
	    -DLEAF_BACKEND_DOUBLES -DLEAF_FLOAT_TYPE=$(LEAF_FLOAT_TYPE) \
	    -include $(LEAF_PRIM_DIR)/leaf_primitive_double.h \
	    -include $(BACKENDS_DIR)/interface_motobuddy.h \
	    -o $@ $(TEST_UNIT_SRC) $(TEST_UNIT_OBJS) \
	    $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a $(CLIBS)

$(TEST_SEM_BIN): $(TEST_SEM_SRC) $(TEST_HARNESS_H) $(TEST_UNIT_OBJS) \
                 $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a
	$(CC) $(INC_DIRS_BUDDY_DOUBLES) -I $(TEST_DIR) $(CFLAGS) \
	    -DLEAF_BACKEND_DOUBLES -DLEAF_FLOAT_TYPE=$(LEAF_FLOAT_TYPE) \
	    -include $(LEAF_PRIM_DIR)/leaf_primitive_double.h \
	    -include $(BACKENDS_DIR)/interface_motobuddy.h \
	    -o $@ $(TEST_SEM_SRC) $(TEST_UNIT_OBJS) \
	    $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a $(CLIBS)

test-circuits:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=3
	bash $(TEST_DIR)/test_circuits.sh

test-benchmarks:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=3
	$(MAKE) $(TEST_SEM_BIN) LEAF_FLOAT_TYPE=3
	@chmod +x $(TEST_DIR)/test_benchmarks.sh
	bash $(TEST_DIR)/test_benchmarks.sh
	$(TEST_SEM_BIN)

$(TEST_META_BIN): $(TEST_META_SRC) $(TEST_HARNESS_H) $(TEST_UNIT_OBJS) \
                  $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a
	$(CC) $(INC_DIRS_BUDDY_DOUBLES) -I $(TEST_DIR) $(CFLAGS) \
	    -DLEAF_BACKEND_DOUBLES -DLEAF_FLOAT_TYPE=$(LEAF_FLOAT_TYPE) \
	    -include $(LEAF_PRIM_DIR)/leaf_primitive_double.h \
	    -include $(BACKENDS_DIR)/interface_motobuddy.h \
	    -o $@ $(TEST_META_SRC) $(TEST_UNIT_OBJS) \
	    $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a $(CLIBS)

test-metamorphic:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=3
	$(MAKE) $(TEST_META_BIN) LEAF_FLOAT_TYPE=3
	$(TEST_META_BIN)

TEST_GROVER_SRC := $(TEST_DIR)/test_grover_matrix.c
TEST_GROVER_BIN := $(BIN_DIR)/test_grover_$(FLOAT_SUFFIX)
TEST_GROVER_GMP_BIN := $(BIN_DIR)/test_grover_gmp

$(TEST_GROVER_BIN): $(TEST_GROVER_SRC) $(TEST_HARNESS_H) $(TEST_UNIT_OBJS) \
                    $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a
	$(CC) $(INC_DIRS_BUDDY_DOUBLES) -I $(TEST_DIR) $(CFLAGS) \
	    -DLEAF_BACKEND_DOUBLES -DLEAF_FLOAT_TYPE=$(LEAF_FLOAT_TYPE) \
	    -include $(LEAF_PRIM_DIR)/leaf_primitive_double.h \
	    -include $(BACKENDS_DIR)/interface_motobuddy.h \
	    -o $@ $(TEST_GROVER_SRC) $(TEST_UNIT_OBJS) \
	    $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a $(CLIBS)

$(TEST_GROVER_GMP_BIN): $(TEST_GROVER_SRC) $(TEST_HARNESS_H) $(TEST_STRESS_GMP_OBJS) \
                        $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a
	$(CC) $(INC_DIRS_BUDDY_GMP) -I $(TEST_DIR) $(CFLAGS) \
	    -DLEAF_BACKEND_GMP \
	    -include $(LEAF_PRIM_DIR)/leaf_primitive_mpz.h \
	    -include $(BACKENDS_DIR)/interface_motobuddy.h \
	    -o $@ $(TEST_GROVER_SRC) $(TEST_STRESS_GMP_OBJS) \
	    $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a $(CLIBS)

test-grover: test-grover-all

test-grover-all: test-grover-f32 test-grover-f64 test-grover-f80 \
                 test-grover-f128 test-grover-gmp

test-grover-f32:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=0
	$(MAKE) $(BIN_DIR)/test_grover_f32 LEAF_FLOAT_TYPE=0
	$(BIN_DIR)/test_grover_f32

test-grover-f64:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=1
	$(MAKE) $(BIN_DIR)/test_grover_f64 LEAF_FLOAT_TYPE=1
	$(BIN_DIR)/test_grover_f64

test-grover-f80:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=2
	$(MAKE) $(BIN_DIR)/test_grover_f80 LEAF_FLOAT_TYPE=2
	$(BIN_DIR)/test_grover_f80

test-grover-f128:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=3
	$(MAKE) $(BIN_DIR)/test_grover_f128 LEAF_FLOAT_TYPE=3
	$(BIN_DIR)/test_grover_f128

test-grover-gmp: buddy_gmp $(TEST_GROVER_GMP_BIN)
	$(TEST_GROVER_GMP_BIN)

test-stress: test-stress-f128 test-stress-gmp

test-stress-f128:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=3
	@rm -f $(TEST_STRESS_F128_BIN)
	$(MAKE) $(TEST_STRESS_F128_BIN) STRESS_LEVEL=$(STRESS_LEVEL) LEAF_FLOAT_TYPE=3
	$(TEST_STRESS_F128_BIN)

test-stress-f64:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=1
	@rm -f $(TEST_STRESS_F64_BIN)
	$(MAKE) $(TEST_STRESS_F64_BIN) STRESS_LEVEL=$(STRESS_LEVEL) LEAF_FLOAT_TYPE=1
	$(TEST_STRESS_F64_BIN)

test-stress-gmp: buddy_gmp
	@rm -f $(TEST_STRESS_GMP_BIN)
	$(MAKE) $(TEST_STRESS_GMP_BIN) STRESS_LEVEL=$(STRESS_LEVEL)
	$(TEST_STRESS_GMP_BIN)

test-leaks:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=3
	$(MAKE) $(TEST_UNIT_BIN) LEAF_FLOAT_TYPE=3
	@chmod +x $(TEST_DIR)/test_leaks.sh
	bash $(TEST_DIR)/test_leaks.sh

test-mutation:
	$(MAKE) buddy_doubles LEAF_FLOAT_TYPE=3
	@chmod +x $(TEST_DIR)/test_mutation.sh
	bash $(TEST_DIR)/test_mutation.sh

$(TEST_STRESS_DOUBLES_BIN): $(TEST_STRESS_SRC) $(TEST_HARNESS_H) $(TEST_UNIT_OBJS) \
                        $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a
	$(CC) $(INC_DIRS_BUDDY_DOUBLES) -I $(TEST_DIR) $(CFLAGS) \
	    -DLEAF_BACKEND_DOUBLES -DLEAF_FLOAT_TYPE=$(LEAF_FLOAT_TYPE) \
	    -DSTRESS_LEVEL=$(STRESS_LEVEL) \
	    -include $(LEAF_PRIM_DIR)/leaf_primitive_double.h \
	    -include $(BACKENDS_DIR)/interface_motobuddy.h \
	    -o $@ $(TEST_STRESS_SRC) $(TEST_UNIT_OBJS) \
	    $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a $(CLIBS)

$(TEST_STRESS_GMP_BIN): $(TEST_STRESS_SRC) $(TEST_HARNESS_H) $(TEST_STRESS_GMP_OBJS) \
                        $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a
	$(CC) $(INC_DIRS_BUDDY_GMP) -I $(TEST_DIR) $(CFLAGS) \
	    -DLEAF_BACKEND_GMP -DSTRESS_LEVEL=$(STRESS_LEVEL) \
	    -include $(LEAF_PRIM_DIR)/leaf_primitive_mpz.h \
	    -include $(BACKENDS_DIR)/interface_motobuddy.h \
	    -o $@ $(TEST_STRESS_SRC) $(TEST_STRESS_GMP_OBJS) \
	    $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a $(CLIBS)

# ==============================================================================
# buddy_gmp target
# ==============================================================================

buddy_gmp: $(OBJS_BUDDY_GMP) $(INTERFACE_OBJ_motobuddy) \
           $(LEAF_OBJ_mpz) $(LEAF_OBJ_algebraic) \
           $(LIB_DIR)/MoToBuddy/build/src/libbuddy.a | $(BIN_DIR)
	$(LINKER) $(INC_DIRS_BUDDY_GMP) $(CFLAGS) -o $(BIN_DIR)/MEDUSA_buddy_gmp $^ $(CLIBS)

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
ifeq ($(LEAF_FLOAT_TYPE),3)
	ln -sfn MEDUSA_buddy_doubles_$(FLOAT_SUFFIX) $(EXEC)
endif

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
# Directory creation
# ==============================================================================

$(BIN_DIR) $(OBJ_DIR) $(OBJ_DIR)/buddy_gmp $(DOUBLES_OBJ_DIR):
	mkdir -p $@

# ==============================================================================
# Dependency inclusion
# ==============================================================================

-include $(OBJS_BUDDY_GMP:.o=.d)
-include $(OBJS_BUDDY_DOUBLES:.o=.d)

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

init: init-motobuddy

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
	       MEDUSA_buddy_gmp \
	       MEDUSA_buddy_mpfr_256 MEDUSA_buddy_mpfr_512 \
	       MEDUSA_sylvan_gmp MEDUSA_sylvan_doubles \
	       $(TEST_UNIT_BIN) $(TEST_STRESS_F64_BIN) $(TEST_STRESS_F128_BIN) \
	       $(TEST_STRESS_GMP_BIN) \
	       $(TEST_SEM_BIN) $(TEST_META_BIN) \
	       $(BIN_DIR)/test_grover_f32 $(BIN_DIR)/test_grover_f64 \
	       $(BIN_DIR)/test_grover_f80 $(BIN_DIR)/test_grover_f128 \
	       $(TEST_GROVER_GMP_BIN)

clean-deps:
	rm -rf $(LIB_DIR)

clean-benchmark:
	cd .. && rm -rf SliQSim
