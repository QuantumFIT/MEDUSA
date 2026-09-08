OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[1];
h qubits[0];
ry(0.0) qubits[0];
ry(1.0) qubits[0];
