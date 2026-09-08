OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[1];
h qubits[0];
rz(2*pi/3) qubits[0];
