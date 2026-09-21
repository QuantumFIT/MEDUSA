# MOSF fixtures

MOSF (MTBDD Operation Serialization Format) is the JSON input path, reached
with `--tree-simulation` and parsed by `sim_mosf_file` (`src/sim_mosf.cpp`).
It is compiled only under `USE_CXX=1`.

Each `NAME.mosf` here is paired with a `NAME.qasm` describing the **same
circuit** in OpenQASM. `tests/test_mosf.sh` runs both and requires the two
`res.dot` files to be identical - the same loop-vs-unrolled shape used in
`test_grover_matrix`, across input formats rather than across simulation
modes. That is what makes these fixtures self-checking: the OpenQASM path is
independently covered by every other suite, so it can serve as the oracle.

The format is specified in `lib/MoToBuddy/doc/mosf.mosf`. Beyond the built-in
op types (`get_side`, `makenode`, `traverse_to`, `swap`, `lockstep_to`,
`received`, `group`), a host registers its own; MEDUSA's registry is defined
at the top of `sim_mosf_file` and provides exactly:

  node ops     neg, i_mul, neg_i_mul, phase_mul, and rx/ry/rz which throw
  binary ops   plus_mulsqrt2, minus_mulsqrt2

Note that `lib/MoToBuddy/examples/quantum_gates/*.mosf` are **gate
definitions**, not circuits: they have no `x_levels` and they use `plus_s` /
`minus_s`, which MEDUSA does not register. They are therefore not usable as
input here, which is why these fixtures had to be written from the spec.

Gate spellings used below:

  H on level L   traverse_to L, makenode(low: plus_mulsqrt2, high: minus_mulsqrt2)
  X on level L   traverse_to L, action swap
  Z on level L   traverse_to L, action neg, action_on R
