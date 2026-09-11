OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[2];

// gate_symb_ry_pihalf: Ry(pi/2)^4 = I.
// Classic oracle for ry_loop.qasm: this file is run with opt_symb=false and
// never symbolically, so it is gates.c unrolled by hand rather than a second
// symbolic run. See assert_symb_matches_classic in tests/test_metamorphic.c.
h qubits[1];
ry(pi/2) qubits[1];
ry(pi/2) qubits[1];
ry(pi/2) qubits[1];
ry(pi/2) qubits[1];
h qubits[1];
