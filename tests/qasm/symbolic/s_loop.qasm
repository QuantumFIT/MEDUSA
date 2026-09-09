OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;

// gate_symb_s: S^4 = I, so H S^4 H |0> must return |0>
h q[0];
for int i in [1:4] {
s q[0];
}
h q[0];
