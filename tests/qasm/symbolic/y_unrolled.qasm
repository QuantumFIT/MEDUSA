OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[2];

// interface_gate_symb_y: Y^2 = I.
// Classic oracle for y_loop.qasm: this file is run with opt_symb=false and
// never symbolically, so it is gates.c unrolled by hand rather than a second
// symbolic run. See assert_symb_matches_classic in tests/test_metamorphic.c.
h qubits[0];
y qubits[0];
y qubits[0];
h qubits[0];
