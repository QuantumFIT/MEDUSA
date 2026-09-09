OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[2];

// gate_symb_ry_pihalf: Ry(pi/2)^4 = I (unrolled reference)
h qubits[1];
ry(pi/2) qubits[1];
ry(pi/2) qubits[1];
ry(pi/2) qubits[1];
ry(pi/2) qubits[1];
h qubits[1];
