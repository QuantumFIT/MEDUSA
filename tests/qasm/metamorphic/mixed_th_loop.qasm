OPENQASM 3.0;
include "stdgates.inc";
qubit[8] q;

// Superposition + T/Tdg mix, then entangle into the high half.
h q[0];
h q[1];
h q[2];
h q[3];
t q[0];
tdg q[1];
t q[2];
tdg q[3];
cx q[0], q[4];
cx q[1], q[5];
cx q[2], q[6];
cx q[3], q[7];
t q[4];
tdg q[5];
t q[6];
tdg q[7];
ccx q[0], q[1], q[4];
ccx q[2], q[3], q[6];
h q[4];
h q[6];

// Five iterations of mixed Clifford+T with CX/CZ/CCX.
for int i in [1:5] {
h q[0];
t q[0];
cx q[0], q[1];
tdg q[1];
h q[2];
t q[2];
cx q[2], q[3];
tdg q[3];
cz q[1], q[5];
cz q[3], q[7];
ccx q[0], q[2], q[4];
t q[4];
tdg q[6];
cx q[4], q[5];
h q[5];
ccx q[1], q[3], q[6];
tdg q[0];
t q[7];
cx q[6], q[7];
}

h q[0];
h q[1];
h q[2];
h q[3];
t q[4];
tdg q[5];
h q[6];
s q[7];
