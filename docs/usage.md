---
title: Usage
---

[Home](./) · [Installation](installation) · [Usage](usage) · [Reference](reference)

# Usage

```console
$ ./MEDUSA [options] --file <circuit.qasm>
```

MEDUSA reads one OpenQASM circuit, simulates it from the all-zero state \|0…0⟩, and writes the final state to `res.dot` in the current directory. Without `--file`, it reads the circuit from standard input. All options are listed in the [command-line reference](reference); this page explains what they are for.

## Input circuits

MEDUSA reads a subset of OpenQASM 2 and 3: one qubit register and at most one bit register of the same size, and the gates below. The `OPENQASM` and `include` lines are accepted and otherwise ignored.

```
OPENQASM 3.0;
include "stdgates.inc";
qubit[3] q;          // or, in OpenQASM 2: qreg q[3];
bit[3] c;            //                     creg c[3];

h q[0];
cx q[0], q[1];
ccx q[0], q[1], q[2];
c[2] = measure q[2]; // or: measure q[2] -> c[2];
```

| Gates | |
|---|---|
| `x`, `y`, `z`, `h`, `s`, `t`, `tdg` | single-qubit gates |
| `rx(θ)`, `ry(θ)`, `rz(θ)` | rotations by an arbitrary angle, written as a number or with `pi` (floating-point builds only) |
| `cx`, `cz` | controlled X and Z |
| `ccx` | Toffoli |
| `mcx` | X with two or three controls; the last qubit is the target |

The `benchmarks/` directory has several hundred circuits to try, in two variants: `no-measure/` and `measure/` (the same circuits with `measure` statements at the end).

## Loops and symbolic simulation

A `for` loop repeats its body. The range is inclusive, `[start:end]` or `[start:step:end]`; the loop variable cannot be used in the body. Loops cannot be nested, and a circuit with loops must be read from a file, not from standard input.

```
for int i in [1:4549] {
    // oracle and diffusion
    ...
}
```

By default, MEDUSA simply runs the body once per iteration. With `--symbolic` (`-s`), it **summarizes** each loop instead: it runs the body on symbolic amplitudes, repeats that pass until the symbolic form stops changing, and then evaluates the summary for all iterations at once. For loops with many iterations, this is where MEDUSA's speed comes from; Grover's search with 4549 iterations goes from about 37 seconds to 0.02 seconds ([example](./#quick-example)).

Inside a summarized loop, only gates with a symbolic form are allowed: `x`, `y`, `z`, `h`, `s`, `t`, `tdg`, `rx(pi/2)`, `ry(pi/2)`, `cx`, `cz`, `ccx` and `mcx`. Any other rotation in a loop body stops the simulation with an error when `--symbolic` is given. Gates outside loops are not restricted.

## The result

The final state goes to **`res.dot`**, a [Graphviz](https://graphviz.org/) description of the decision diagram. Each round node is a qubit, with a dashed edge for 0 and a solid edge for 1. Each box is an amplitude, written as *re*+*im*i. A path from the top to a box spells out one or more basis states and their amplitude; a qubit that is skipped on the path can be either 0 or 1.

```console
$ ./MEDUSA --file benchmarks/no-measure/BernsteinVazirani/01.qasm
$ make plot                  # res.dot -> res.pdf (needs Graphviz)
```

- **`--probability`** (`-p`) writes \|amplitude\|² into the boxes instead of the amplitudes.
- With the exact algebraic build (`MEDUSA_buddy_gmp`), amplitudes whose integer coefficients are very large are written as variables in `res.dot`, and their values go to **`res-vars.txt`**.

Converting a large `res.dot` to an image can take a long time.

## Measurement

`measure` statements in the circuit say which qubits are measured into which bits. MEDUSA takes the measurements only when asked to with `--measure` (`-m`), and then at the end of the circuit: it samples the final state `--nsamples` times (1024 by default) and prints how often each outcome occurred.

```console
$ ./MEDUSA --file benchmarks/measure/BernsteinVazirani/01.qasm --measure --nsamples 100
Sampled results:
    '11'    100
```

`--measure FILE` writes the samples to a file instead. Without `--measure`, `measure` statements have no effect, and `res.dot` holds the state before measurement.

## Time, memory and numerical error

- **`--info`** (`-i`) prints the simulation time and the peak memory use.
- **`--norm-error`** (`-e`) checks after every gate how far the norm of the state has drifted from 1, a measure of accumulated floating-point error, and writes the result per gate to a CSV file (`norm_track.csv`, or the file given with `--norm-csv`).
