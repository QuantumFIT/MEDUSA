OPENQASM 3.0;
include "stdgates.inc";
qubit[3] q;

// gate_symb_toffoli branch t0 < c1 < c2: target above both controls
h q[1];
h q[2];
for int i in [1:2] {
ccx q[1], q[2], q[0];
}
h q[1];
h q[2];
