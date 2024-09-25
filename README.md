# relaxed-bp
Relaxed belief propagation project

# mcj-group build and run

## Software-only (non-Swarm) versions

As a baseline we compare performance against software-only sequential or
parallel implementations, which we call "competition". These do not use any
Swarm features, instead targeting a uniprocessor or multicore. The most relevant
comparison for now are the C++ sequential implementations. Note that this repo
also contains several sequential and parallel Java implementations from
[[Aksenov+, NeurIPS
2020]](https://papers.nips.cc/paper/2020/file/fdb2c3bab9d0701c4a050a4d8d782c7f-Paper.pdf).


### Build competition

```bash
cd [/path/to/]swarm-benchmarks
scons runtime=competition --gcc -j4 relaxed-bp
```

If you have more than 4 cores on your machine, increase the number beside `-j`.
If you try to build before installing the java compiler, following installation,
you will need to destroy the SCons cache so that it re-learns about javac:

```bash
cd [/path/to/]swarm-benchmarks
rm -r .scon*
```


### Run competition in the simulator

```bash
cd [/path/to/]swarm-benchmarks
[/path/to/]sim/build/opt/sim/sim -config [/path/to/]sim/configs/sample.cfg -- ./build/release_competition/relaxed-bp/bp residual-inserts-only ising 100
```


### Run competition natively

When doing application development, you initially want to verify that your
application is correct, and iterate quickly. At this stage, you do not need the
statistics, configurability, and precision that the simulator offers, so why pay
the orders of magnitude slowdown required to simulate? You can build, run, and
debug non-Swarm (aka "competition") applications for native execution, as shown
below, where `--allocator=native` is the key enabler. Notably, this uses the
system's native memory allocator (probably libc's malloc), rather than
`simalloc` which uses a memory allocator internal to the simulator. These
execute almost the same code as the example above, but are much faster in wall
clock time because they are not simulated.

```bash
cd [/path/to/]swarm-benchmarks
scons runtime=competition --gcc --allocator=native -j4 relaxed-bp
./build/release_competition/relaxed-bp/bp residual-inserts-only ising 100
./build/release_competition/relaxed-bp/bp residual ising 100
# You can also run Java code,
# although it doesn't have ROI hooks with the simulator
cd build/release_competition/relaxed-bp/classes
java bp.testing.Main residual ising 100 1 2
```


## Swarm versions

We have one Swarm implementation with *coarse-grain* tasks that does not yet
scale well. Further implementations will refine it for better scaling and
performance.

### Build and run in the simulator

```bash
cd [/path/to/]swarm-benchmarks
scons --gcc -j4 relaxed-bp
[/path/to/]sim/build/opt/sim/sim -config [/path/to/]sim/sample.cfg -- ./build/release/relaxed-bp/bp swarm-cg ising 100
```


### Build and run natively

The Swarm execution model mimics a sequential loop popping and pushing to
a global priority queue. Therefore for fast iteration in application
development, test, and debug, we can sidestep the simulator and build the
Swarm application in what is called the "sequential runtime", which is simply a
sequential program popping and pushing from a global priority queue. Running
this natively should be much faster than running a program in the simulator. Use
this for test and debug.

```bash
cd [/path/to/]swarm-benchmarks
scons runtime=seq --allocator=native --gcc -j4 relaxed-bp
./build/release_seq/relaxed-bp/native_bp swarm-cg ising 100
```

This should be about as fast to run as the native software-only sequential
version above.
