OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[2];

// gate_symb_cnot branch xt < xc: target q[0] above control q[1].
// Classic oracle for cx_t_above_loop.qasm: this file is run with opt_symb=false and
// never symbolically, so it is gates.c unrolled by hand rather than a second
// symbolic run. See assert_symb_matches_classic in tests/test_metamorphic.c.
h qubits[0];
h qubits[1];
cx qubits[1], qubits[0];
t qubits[0];
cx qubits[1], qubits[0];
t qubits[0];
h qubits[1];
