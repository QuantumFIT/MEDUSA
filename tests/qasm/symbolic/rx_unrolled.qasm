OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[2];

// gate_symb_rx_pihalf: Rx(pi/2)^4 = I (unrolled reference)
h qubits[1];
rx(pi/2) qubits[1];
rx(pi/2) qubits[1];
rx(pi/2) qubits[1];
rx(pi/2) qubits[1];
h qubits[1];
