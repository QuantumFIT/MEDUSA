OPENQASM 3.0;
include "stdgates.inc";
qubit[3] q;

// gate_symb_toffoli branch c1 < t0 < c2: target between the controls
h q[0];
h q[2];
for int i in [1:2] {
ccx q[0], q[2], q[1];
}
h q[0];
h q[2];
