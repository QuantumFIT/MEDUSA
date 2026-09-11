OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[2];

// gate_symb_s: S^4 = I, so H S^4 H |0> must return |0>.
// Classic oracle for s_loop.qasm: this file is run with opt_symb=false and
// never symbolically, so it is gates.c unrolled by hand rather than a second
// symbolic run. See assert_symb_matches_classic in tests/test_metamorphic.c.
h qubits[0];
s qubits[0];
s qubits[0];
s qubits[0];
s qubits[0];
h qubits[0];
