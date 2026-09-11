OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[3];

// gate_symb_toffoli branch c1 < t0 < c2: target between the controls.
// Classic oracle for ccx_t_mid_loop.qasm: this file is run with opt_symb=false and
// never symbolically, so it is gates.c unrolled by hand rather than a second
// symbolic run. See assert_symb_matches_classic in tests/test_metamorphic.c.
h qubits[0];
h qubits[2];
ccx qubits[0], qubits[2], qubits[1];
ccx qubits[0], qubits[2], qubits[1];
h qubits[0];
h qubits[2];
