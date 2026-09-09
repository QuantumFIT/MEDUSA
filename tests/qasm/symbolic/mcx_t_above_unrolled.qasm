OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[3];

// gate_symb_mcx branch above_t non-empty: controls above the target (unrolled reference)
h qubits[1];
h qubits[2];
mcx qubits[1], qubits[2], qubits[0];
mcx qubits[1], qubits[2], qubits[0];
h qubits[1];
h qubits[2];
