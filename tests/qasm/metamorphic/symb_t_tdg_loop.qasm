OPENQASM 3.0;
include "stdgates.inc";
qubit[1] q;

h q[0];
for int i in [1:2] {
t q[0];
tdg q[0];
}
h q[0];
