OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[2];

// gate_symb_s: S^4 = I, so H S^4 H |0> must return |0> (unrolled reference)
h qubits[0];
s qubits[0];
s qubits[0];
s qubits[0];
s qubits[0];
h qubits[0];
