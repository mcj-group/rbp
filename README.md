
# Relaxed Residual Belief Propagation with the Stealing Multiqueue and Lazy Priority Updates

```
@inproceedings{bassi:pgm26rbpsmq,
    title     ={Optimized Priority Scheduling for Faster Scalable Belief Propagation},
    author    ={Bassi, Abnash and Posluns, Gilead and Jeffrey, Mark C.},
    booktitle ={Proc. 13th International Conference on Probabilistic Graphical Models},
    series    ={PGM},
    year      ={2026},
    url       ={https://openreview.net/forum?id=iATKlKvddx}
}
```

## Steps to reproduce experiment results

### Setup and Compilation
1. On a Debian/Ubuntu machine, install the Boost library using `sudo apt install libboost-all-dev`. If you are using a different OS, follow [the corresponding instructions here](https://www.boost.org/doc/user-guide/getting-started.html). We recommend Boost version 1.85.0.
2. `mkdir golden` to store residual algorithm results for accuracy comparisons
3. `rm -rf build`
4. `make`

### Evaluation

#### Examples:
```
./build/main residual ising default 2
./build/main relaxed-rbp-mq ising default 48 2
./build/main relaxed-rbp-smq ising default 48 2
./build/main synch_bp_mt ising default 48 2
``` 

#### General command syntax: 
```
./build/main <algorithm> <mrf> <size> [all except residual: <threads>] [optional: <seed>]
```

#### Argument Options:

algorithms: `residual`, `relaxed-rbp-mq`, `relaxed-rbp-smq`, `relaxed-smart-splash-mq`, `relaxed-smart-splash-smq`, `synch_bp_mt`

mrf: `ising`, `potts`, `ldpc`, `tree`

size: `default` to match paper results or other value

threads: machine-dependent

seed: `2` to match paper results or other value
