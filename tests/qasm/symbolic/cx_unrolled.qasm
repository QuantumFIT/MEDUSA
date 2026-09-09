OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[2];

// gate_symb_cnot: CX with a T on the target inside the loop (unrolled reference)
h qubits[0];
h qubits[1];
cx qubits[0], qubits[1];
t qubits[1];
cx qubits[0], qubits[1];
t qubits[1];
h qubits[0];
