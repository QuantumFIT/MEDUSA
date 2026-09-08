OPENQASM 3.0;
include "stdgates.inc";
qubit[1] q;

h q[0];
for int i in [1:1] {
rx(1.0) q[0];
}
