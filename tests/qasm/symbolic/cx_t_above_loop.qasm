OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;

// gate_symb_cnot branch xt < xc: target q[0] above control q[1]
h q[0];
h q[1];
for int i in [1:2] {
cx q[1], q[0];
t q[0];
}
h q[1];
