OPENQASM 3.0;
include "stdgates.inc";
qubit[3] q;

// gate_symb_mcx branch above_t non-empty: controls above the target
h q[1];
h q[2];
for int i in [1:2] {
mcx q[1], q[2], q[0];
}
h q[1];
h q[2];
