OPENQASM 3.0;
include "stdgates.inc";
qubit[3] q;

// gate_symb_mcx: MCX^2 = I with two controls
h q[0];
h q[1];
for int i in [1:2] {
mcx q[0], q[1], q[2];
}
h q[0];
h q[1];
