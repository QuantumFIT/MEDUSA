OPENQASM 2.0;
include "qelib1.inc";
qreg qubits[3];

// gate_symb_mcx branch above_t non-empty: controls above the target.
// Classic oracle for mcx_t_above_loop.qasm: this file is run with opt_symb=false and
// never symbolically, so it is gates.c unrolled by hand rather than a second
// symbolic run. See assert_symb_matches_classic in tests/test_metamorphic.c.
h qubits[1];
h qubits[2];
mcx qubits[1], qubits[2], qubits[0];
mcx qubits[1], qubits[2], qubits[0];
h qubits[1];
h qubits[2];
