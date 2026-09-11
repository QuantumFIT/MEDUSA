OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[2];

// gate_symb_cnot: CX with a T on the target inside the loop.
// Classic oracle for cx_loop.qasm: this file is run with opt_symb=false and
// never symbolically, so it is gates.c unrolled by hand rather than a second
// symbolic run. See assert_symb_matches_classic in tests/test_metamorphic.c.
h qubits[0];
h qubits[1];
cx qubits[0], qubits[1];
t qubits[1];
cx qubits[0], qubits[1];
t qubits[1];
h qubits[0];
