#!/bin/bash

# ======================================================================
# This script pulls benchmarks from AutoQ's repository and makes all
# necessary modifications.
#
# It is assumed that the script is run from the repository's home folder.
# ======================================================================

BSCRIPT_PATH=../benchmark-utils/scripts

git clone https://github.com/alan23273850/AutoQ.git || true &&\
mv AutoQ/benchmarks . && rm -rf AutoQ &&\
cd benchmarks &&\
rm -rf Symbolic* && find . -type f -not -name '*.qasm' -delete

# remove unnecessary folder structure
for file in ./*/*/*.qasm; do
    TEST_FD=${file%/*.qasm}    # eg. ./Grover/02
    NEW_PAR_FD=${TEST_FD%/*}   # eg. ./Grover
    TEST_N=${TEST_FD##*/}      # eg. 02
    mv $file $NEW_PAR_FD/$TEST_N.qasm
    rm -rf $TEST_FD
done

. "$BSCRIPT_PATH/add-measure.sh"