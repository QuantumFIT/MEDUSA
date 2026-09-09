OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[3];

// gate_symb_toffoli branch t0 < c1 < c2: target above both controls (unrolled reference)
h qubits[1];
h qubits[2];
ccx qubits[1], qubits[2], qubits[0];
ccx qubits[1], qubits[2], qubits[0];
h qubits[1];
h qubits[2];
