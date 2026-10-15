---
---

[Home](./) · [Installation](installation) · [Usage](usage) · [Reference](reference)

**MEDUSA** (**M**ulti-T**e**rminal Binary **D**ecision Diagram-based q**U**antum circuit **S**imul**A**tor) simulates quantum circuits written in OpenQASM. It represents the quantum state as a **multi-terminal binary decision diagram** (MTBDD), and it can **summarize loops symbolically**: a loop that repeats a block of gates thousands of times is simulated with a few passes over the block instead of one pass per iteration.

## How it works

The state of an *n*-qubit circuit is a vector of 2<sup>*n*</sup> complex amplitudes. MEDUSA stores it as an MTBDD: a decision diagram with one level per qubit and the amplitudes in its leaves. Identical sub-vectors are shared, so states with structure, such as basis states, uniform superpositions and the states of reversible and Grover-like circuits, stay small even for many qubits. Gates are applied as operations on the diagram.

- **Symbolic loop summarization.** With `--symbolic`, MEDUSA runs the body of a `for` loop on symbolic amplitudes, repeating the pass over the body until the symbolic form stops changing (usually a handful of passes), and then evaluates that summary for the full number of iterations at once. The loop body must use gates that have a symbolic form, which covers the Clifford+T gates, `rx(pi/2)`, `ry(pi/2)` and the multi-controlled X ([details](usage#loops-and-symbolic-simulation)).
- **Choice of number representation.** Amplitudes are complex numbers with a floating-point real and imaginary part: 32-, 64-, 80- or 128-bit (the default). An exact **algebraic** representation, (*a* + *b*ω + *c*ω² + *d*ω³) / √2<sup>*k*</sup> with integers *a*–*d*, *k* and ω = e<sup>iπ/4</sup>, computes Clifford+T circuits with no rounding at all. The representation is chosen when MEDUSA is built.
- **Two decision diagram libraries.** [MoToBuddy](https://github.com/VeriFIT/MoToBuddy), the default, and optionally [Sylvan](https://github.com/trolando/sylvan), for comparison.

## Quick example

Grover's search on 25 data qubits (50 qubits in total) repeats its oracle and diffusion 4549 times in a `for` loop. MEDUSA simulates it gate by gate in about half a minute, and with loop summarization in a few hundredths of a second:

```console
$ f=benchmarks/no-measure/LP-Grover/25.qasm
$ ./MEDUSA --file $f --info
Time=36.8s
Peak Memory Usage=5120kB
$ ./MEDUSA --file $f --info --symbolic
Time=0.0206s
Peak Memory Usage=6400kB
```

(Without `--symbolic`, the decision diagram library also prints a line for each garbage collection, about 38,000 of them here; they are left out above.)

The resulting state is written to `res.dot` as a [Graphviz](https://graphviz.org/) diagram.

Continue with [Installation](installation) and [Usage](usage).

## Citing MEDUSA

If MEDUSA contributes to academic work, please cite the paper it is based on:

> Tian-Fu Chen, Yu-Fang Chen, Jie-Hong Roland Jiang, Sára Jobranová, and Ondřej Lengál. *Accelerating Quantum Circuit Simulation with Symbolic Execution and Loop Summarization*. In Proceedings of the 43rd IEEE/ACM International Conference on Computer-Aided Design (ICCAD '24), pages 1–9. ACM, 2024. [doi:10.1145/3676536.3676711](https://doi.org/10.1145/3676536.3676711)

```bibtex
@inproceedings{medusa-iccad24,
  author    = {Chen, Tian-Fu and Chen, Yu-Fang and Jiang, Jie-Hong Roland and
               Jobranov{\'a}, S{\'a}ra and Leng{\'a}l, Ond{\v r}ej},
  title     = {Accelerating Quantum Circuit Simulation with Symbolic Execution
               and Loop Summarization},
  booktitle = {Proceedings of the 43rd IEEE/ACM International Conference on
               Computer-Aided Design (ICCAD '24)},
  publisher = {Association for Computing Machinery},
  year      = {2024},
  pages     = {1--9},
  doi       = {10.1145/3676536.3676711}
}
```

MEDUSA is released under the [MIT License](https://github.com/QuantumFIT/MEDUSA/blob/devel/LICENSE). Contributors are listed in [`AUTHORS`](https://github.com/QuantumFIT/MEDUSA/blob/devel/AUTHORS).
