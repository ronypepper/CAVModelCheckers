# Computer-Aided Verification Model Checkers

This repository contains a bounded model checker and an interpolated model checker using MiniSAT.
It can parse and solve sequential circuits in AIGer text format.

To build, run:
```
mkdir build
cd build
cmake ..
make
```
This creates three executables:
- **bmc** ..... Bounded Model Checker
  - Parameters:
    - Initial unwinding depth k
    - Target .aag file
- **imc** ..... Interpolated Model Checker
  - Parameters:
      - Initial unwinding depth k
      - Timeout in seconds (only checked before SAT solving; non-positive values mean no timeout)
      - Target .aag file
- **imc_batch** ..... Batched version of the Interpolated Model Checker
  - Recursively searches a directory for .aag files, runs the Interpolated Model Checker and compares the result to an expected result, which must be indicated by stating SATISFIABLE or UNSATISFIABLE in the last line of the AIGer file.
  - Can be used to verify the model checker on the AIGer files of this repo: https://github.com/tniessen/aiger-safety-properties
  - Parameters:
      - Initial unwinding depth k
      - Timeout in seconds (only checked before SAT solving; non-positive values mean no timeout)
      - Maximum number of variables in one state of the model allowed, skip file otherwise (non-positive value means no maximum)
      - Target directory
