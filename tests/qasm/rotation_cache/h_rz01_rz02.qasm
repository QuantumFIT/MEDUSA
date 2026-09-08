OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[1];
h qubits[0];
rz(0.1) qubits[0];
rz(0.2) qubits[0];
