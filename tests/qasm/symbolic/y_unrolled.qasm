OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[2];

// interface_gate_symb_y: Y^2 = I (unrolled reference)
h qubits[0];
y qubits[0];
y qubits[0];
h qubits[0];
