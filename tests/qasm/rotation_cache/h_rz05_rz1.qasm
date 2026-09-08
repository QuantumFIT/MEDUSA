OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[1];
h qubits[0];
rz(0.5) qubits[0];
rz(1.0) qubits[0];
