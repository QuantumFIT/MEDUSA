OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
x q[1];
h q[1];
x q[0];
h q[0];
