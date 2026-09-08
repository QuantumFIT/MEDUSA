OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[1];
h qubits[0];
rz(pi/4) qubits[0];
