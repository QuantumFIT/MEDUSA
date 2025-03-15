SRC_DIR:=src
OBJ_DIR:=obj
BIN_DIR:=.
LIB_DIR:=lib
LACE_DIR:=$(LIB_DIR)/sylvan/build/_deps

SRCS:=$(wildcard $(SRC_DIR)/*.c)
EXEC:=$(BIN_DIR)/MEDUSA
OBJS:=$(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

CC:=gcc
CFLAGS:=-g -O2
CLIBS=-lgmp -lpthread -lm
INC_DIRS:=-I $(LIB_DIR)/sylvan/src/ -I $(LACE_DIR)/lace-src/src/ -I $(LACE_DIR)/lace-build/

N_JOBS=4

OF_TYPE=pdf
F_OUT_NAME=res
LONG_NUMS_OUT_FILE=res-vars.txt
UPDATE_OUT_FILE=update.txt
BSCRIPT_PATH=benchmark-utils/scripts

.DEFAULT : all
.PHONY : clean clean-all clean-artifacts clean-deps clean-benchmark plot benchmarks \
           init make-sylvan download-sylvan make-sliqsim

all: $(OBJS) $(LIB_DIR)/sylvan/build/src/lib/libsylvan.a $(LACE_DIR)/lace-build/lib/liblace.a | $(BIN_DIR)
	$(CC) $(INC_DIRS) $(CFLAGS) -o $(EXEC) $^ $(CLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(INC_DIRS) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

-include $(OBJ:.o=.d)

plot:
	@dot -T$(OF_TYPE) $(F_OUT_NAME).dot -o $(F_OUT_NAME).$(OF_TYPE)

benchmarks:
	@bash ./$(BSCRIPT_PATH)/run-benchmarks.sh

# BENCHMARK INIT:
make-sliqsim:
	cd .. &&\
	git clone https://github.com/NTU-ALComLab/SliQSim.git || true &&\
	cd SliQSim/cudd &&\
	./configure --enable-dddmp --enable-obj --enable-shared --enable-static &&\
	cd .. &&\
	make

# INIT:
init: make-sylvan
	mkdir $(LIB_DIR) && mv sylvan $(LIB_DIR)

make-sylvan: download-sylvan
	cd sylvan;			\
	mkdir build;		\
	cd build;			\
	cmake ..;			\
	make -j $(N_JOBS);

download-sylvan:
	@git clone https://github.com/trolando/sylvan.git || true

# CLEAN:
clean: clean-artifacts

clean-all: clean-artifacts clean-deps clean-benchmark

clean-artifacts:
	rm -rf $(EXEC) $(F_OUT_NAME).dot $(F_OUT_NAME).$(OF_TYPE) $(UPDATE_OUT_FILE) $(LONG_NUMS_OUT_FILE) $(OBJ_DIR)

clean-deps:
	rm -rf $(LIB_DIR)

clean-benchmark:
	cd .. && rm -rf SliQSim