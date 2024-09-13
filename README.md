# relaxed-bp
This is a minimalistic version of the original relaxed-bp repo.
The original repo used java and we use C++ instead.
It only contains necessary C++ files adapted from the original java implementation
for relaxed residual belief propagation.

`heap_rbp.cpp`, `bucket_rbp.cpp` contain the MultiQueue-based implementations of relaxed RBP.

These implementations also rely on the MultiQueue files from the cps repo.

# Prerequisites
The executables are built with CMake 3.27.7, GCC 12 and boost >= 1.58.0.



# Build & Run
```
# build and make a build/ folder
./build.sh

# generate a sample output file for the desired model & size
# with the original residual belief propagation,
# to be used as comparison before running relaxed versions
./build/rbp residual ising 1000 0 0 0 0 0 0 0 0

# run experiments
./run.sh

# do a single run with MBQ
# e.g.: ./build/rbp <algorithm> <mrf> <size> <threads> <queues> <batchPop> <batchPush> <delta> <buckets> <usePrefetch> <stickiness>
./build/rbp bucket ising 1000 1 4 128 128 7 64 1 1
```

