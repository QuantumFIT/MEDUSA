OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[3];

// gate_symb_mcx: MCX^2 = I with two controls.
// Classic oracle for mcx_loop.qasm: this file is run with opt_symb=false and
// never symbolically, so it is gates.c unrolled by hand rather than a second
// symbolic run. See assert_symb_matches_classic in tests/test_metamorphic.c.
h qubits[0];
h qubits[1];
mcx qubits[0], qubits[1], qubits[2];
mcx qubits[0], qubits[1], qubits[2];
h qubits[0];
h qubits[1];
