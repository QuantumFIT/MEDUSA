OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[3];

// gate_symb_toffoli branch c1 < t0 < c2: target between the controls (unrolled reference)
h qubits[0];
h qubits[2];
ccx qubits[0], qubits[2], qubits[1];
ccx qubits[0], qubits[2], qubits[1];
h qubits[0];
h qubits[2];
