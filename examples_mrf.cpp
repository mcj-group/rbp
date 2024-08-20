#include <array>
#include <cmath>
#include <random>

#include "examples_mrf.h"
#include "mrf.h"

MRF* examples_mrf::isingMRF(
        uint64_t n, uint64_t m, uint64_t C, uint64_t seed) {
    std::default_random_engine e(seed);
    std::uniform_real_distribution<double> randomHalfCSpread(-0.5 * C, 0.5 * C);

    MRF* mrf = new MRF(n * m, 2 * n * m);

    for (uint64_t i = 0; i < n * m; i++) {
        std::array<double,2> potential;
        double beta = randomHalfCSpread(e);
        for (uint64_t j = 0; j < 2; j++) {
            int v = 2 * j - 1;
            potential[j] = std::exp(beta * v);
        }
        mrf->setNodePotential(i, potential);
    }

    uint64_t dx[] = {1, 0};
    uint64_t dy[] = {0, 1};

    for (uint64_t x = 0; x < n; x++) {
        for (uint64_t y = 0; y < m; y++) {
            uint64_t i = x * m + y;
            for (int k = 0; k < 2; k++) {
                if (x + dx[k] < n && y + dy[k] < m) {
                    uint64_t j = (x + dx[k]) * m + (y + dy[k]);
                    double alpha = randomHalfCSpread(e);
                    std::array<std::array<double,2>,2> potential;
                    for (int a = 0; a < 2; a++) {
                        for (int b = 0; b < 2; b++) {
                            // N.B. these must be signed!
                            int vi = 2 * a - 1;
                            int vj = 2 * b - 1;
                            potential[a][b] = std::exp(alpha * vi * vj);
                        }
                    }
                    mrf->addEdge(i, j, potential);
                }
            }
        }
    }

    return mrf;
}

MRF* examples_mrf::pottsMRF(
        uint64_t n, uint64_t C, uint64_t seed) {
    std::default_random_engine e(seed);
    std::uniform_real_distribution<double> randomHalfCSpread(-0.5 * C, 0.5 * C);

    MRF* mrf = new MRF(n * n, 2 * n * n);

    for (uint64_t i = 0; i < n * n; i++) {
        std::array<double,2> potential;
        double beta = randomHalfCSpread(e);
        potential[0] = 1;
        potential[1] = std::exp(beta);
        mrf->setNodePotential(i, potential);
    }

    uint64_t dx[] = {1, 0};
    uint64_t dy[] = {0, 1};

    for (uint64_t x = 0; x < n; x++){
        for (uint64_t y = 0; y < n; y++){
            uint64_t i = x * n + y;
            for (int k = 0; k < 2; k++){
                if (x + dx[k] < n && y + dy[k] < n){
                    uint64_t j = (x + dx[k]) * n + (y + dy[k]);
                    double alpha = randomHalfCSpread(e);
                    std::array<std::array<double,2>,2> potential;
                    for (int vali = 0; vali < 2; vali ++){
                        for (int valj = 0; valj < 2; valj++){
                            if (vali == valj){
                                potential[vali][valj] = std::exp(alpha);
                            } else {
                                potential[vali][valj] = 1;
                            }
                        }
                    }
                    mrf->addEdge(i, j, potential);
                }
            }
        }
    }
    return mrf;
}

MRF* examples_mrf::randomTree(uint64_t n, uint64_t C, uint64_t seed){
    std::default_random_engine e(seed);
    std::uniform_real_distribution<double> randomZeroToC(0.0, C);
    MRF* mrf = new MRF(n, 2 * n);

    for (uint64_t i = 0; i  < n; i++){
        std::array<double, 2> potential;
        for (uint64_t j = 0; j < 2; j++){
            potential[j] = randomZeroToC(e);
        }
        mrf->setNodePotential(i, potential);
    }
    
    for (uint64_t i = 1; i < n; i++){
        std::uniform_int_distribution<uint64_t> randomToI(0, i-1);
        uint64_t p = randomToI(e);
        std::array<std::array<double, 2>, 2> potential;
        for (int a = 0; a < 2; a++){
            for (int b = 0; b < 2; b++){
                potential[a][b] = randomZeroToC(e);
            }
        }
        mrf->addEdge(i, p, potential);
    }
    return mrf;
}

MRF* examples_mrf::deterministicTree(uint64_t n){
    MRF* mrf = new MRF(n, 2 * n);
    mrf->setNodePotential(0, {0.1, 0.9});
    for (uint64_t i = 1; i < n; i++){
        mrf->setNodePotential(i, {0.5, 0.5});
    }
    std::array<std::array<double, 2>, 2> tmp;
    for (int a = 0; a < 2; a++){
        for (int b = 0; b < 2; b++){
            tmp[a][b] = (a==b);
        }
    }
    for (uint64_t i = 2; i <=n; i++){
        mrf->addEdge(i - 1, i / 2 - 1, tmp);
    }
    return mrf;
}