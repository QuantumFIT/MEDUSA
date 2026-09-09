OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[2];

// gate_symb_cnot branch xt < xc: target q[0] above control q[1] (unrolled reference)
h qubits[0];
h qubits[1];
cx qubits[1], qubits[0];
t qubits[0];
cx qubits[1], qubits[0];
t qubits[0];
h qubits[1];
