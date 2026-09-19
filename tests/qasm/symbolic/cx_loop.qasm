OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;

// gate_symb_cnot: CX with a T on the target inside the loop
h q[0];
h q[1];
for int i in [1:2] {
cx q[0], q[1];
t q[1];
}
h q[0];
