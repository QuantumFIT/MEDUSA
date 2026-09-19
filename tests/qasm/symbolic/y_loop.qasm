OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;

// interface_gate_symb_y: Y^2 = I
h q[0];
for int i in [1:2] {
y q[0];
}
h q[0];
