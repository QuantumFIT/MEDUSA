OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[3];

// gate_symb_mcx: MCX^2 = I with two controls (unrolled reference)
h qubits[0];
h qubits[1];
mcx qubits[0], qubits[1], qubits[2];
mcx qubits[0], qubits[1], qubits[2];
h qubits[0];
h qubits[1];
