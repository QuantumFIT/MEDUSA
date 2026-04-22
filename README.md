# MEDUSA: An MTBDD-based quantum circuit simulator
**MEDUSA** (**M**ulti-Terminal Binary D**E**cision **D**iagram-based Q**U**antum **S**imul**A**tor) is an MTBDD-based quantum circuit simulator supporting `OpenQASM` circuits.
It is written in C and utilizes the [Sylvan](https://trolando.github.io/sylvan/) library for custom MTBDDs and MTBDD operations.

## Build
**Dependencies:**
* [`gmp`](https://gmplib.org/) library
* [`Sylvan`](https://trolando.github.io/sylvan/) and therefore [`Lace`](https://fmt.ewi.utwente.nl/tools/lace/) libraries - can be installed via `make` (needs `git`), see below for more details
* [`FLINT`](https://flintlib.org/doc/) (also installed via `make`) and its dependencies - besides `gmp` also[`mpfr`](https://www.mpfr.org/) library, and GNU build system

Before trying to build this project, make sure to have `gmp` library, `mpfr` library, and GNU build system installed on your machine. You can install them with
```
apt install libgmp-dev libmpfr-dev make autoconf libtool-bin
```
Then you can download the remaining dependencies with:
```
make init
```
Finally, you can build the project with:
```
make
```

## Usage
The simulator accepts input files in the `OpenQASM` format, several circuit files can be found in the `benchmarks` directory:
```
./MEDUSA --file benchmarks/no-measure/BernsteinVazirani/01.qasm 
```
You can also run the simulator with the flag `--info` to print runtime (wall-clock time) and peak physical memory usage to the standard output.
MEDUSA also supports symbolic loop simulation which can be enabled using `--symbolic`. You can find more information about program options with `--help`.
</br>

The result of the simulation can be found in file `res.dot`. Beware that as these files can be quite large, it can take a while to convert them into a graph with a tool such as [`Graphviz`](https://graphviz.org/). This can be done with: 
```
make plot
```
Sometimes the resulting numbers can be very large. In this case substitute variables are used for these numbers in `res.dot`. The values of these variables can be found in file `res-vars.txt`.

</br>