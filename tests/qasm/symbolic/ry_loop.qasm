OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;

// gate_symb_ry_pihalf: Ry(pi/2)^4 = I
h q[1];
for int i in [1:4] {
ry(pi/2) q[1];
}
h q[1];
