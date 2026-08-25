#pragma once
#include <vector>
#include <array>
#include <random>
#include <algorithm>
#include <bits/stdc++.h>
#include <set>
#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <stdlib.h>

#include "mrf.h"

namespace ldpc {
    class Code {
        public:
            std::vector<int> permutation;
            uint64_t n,m,k,l;
    };

    constexpr int gcd(uint64_t a, uint64_t b) {
        return (b == 0) ? a : gcd(b, a % b);
    }

    static inline ldpc::Code* generateLDPCCode(uint64_t n, uint64_t k, uint64_t l, uint64_t seed) {
        ldpc::Code* code = new ldpc::Code;
        uint64_t g = gcd(k,l);
        n -= n % (l/g);
        uint64_t m = n * k / l;
        std::random_device rd;
        std::mt19937 rnd(seed);
        code->n = n; // codeword length
        code->k = k; // 3
        code->l = l; // 6
        code->m = m; // 500

        // std::ifstream inputFile("permutation.txt");

        // if (inputFile.is_open()) {
        //     std::string line;
        //     while (std::getline(inputFile, line)) {
        //         int number = std::atoi(line.c_str()); // Convert string to integer
        //         code->permutation.push_back(number);
        //     }
        //     inputFile.close();
        // } else {
        //     std::cerr << "Unable to open file\n";
        // }

        code->permutation.resize(n*k);
        std::iota(code->permutation.begin(),code->permutation.end(),0);
        bool good = false;
        while (!good) {
            std::shuffle(code->permutation.begin(),code->permutation.end(),rnd);
            good = true;
            for (uint64_t i=0; i<m; i++) {
                std::set<int> pairs;
                for (uint64_t j=0; j<l; j++) {
                    pairs.insert(code->permutation.at(i*l+j)/k);
                }
                if (pairs.size()!=l) {
                    good = false;
                }
            }
        }

        return code;
    }

    static MRF_CSR* generateMRF(ldpc::Code* code, double e, uint64_t seed) {
        std::mt19937 gen(seed);
        std::uniform_real_distribution<> dis(0.0, 1.0);

        uint64_t n = code->n; // 1000
        uint64_t k = code->k; // 3
        uint64_t l = code->l; // 6
        uint64_t m = code->m; // 500

        std::vector<int> permutation = code->permutation;

        int errors = 0;
        MRF_CSR *mrf = new MRF_CSR(n,m,2*(n+m));
        std::vector<int> y(n,0);

        //std::vector<int> flip_locs = {10, 90, 100, 103, 300, 340, 500, 600, 709, 809, 880, 900};

        // Setting up VAR Node potential
        for (uint64_t i=0; i<n; i++) {
            if (dis(gen) < e) {// insert errors by flipping one bit
                y[i]=1;
                errors++;
            }
            // a single row vector: tmp var to hold  log-likelihood ratio
            std::vector<double> potentials(2);
            potentials[y.at(i)] = 1-e;
            potentials[1-y.at(i)] = e;
            mrf->setVarNodePotential(i, potentials);
        }
        std::cout << "Number of Errors: " << errors << std::endl;

        // Just assign a random number to some random locations
        for (uint64_t i=0; i<m; i++) {
            std::vector<double> potentials(1<<l,0.0); // a row vector
            for (uint64_t mask=0; mask < (1U << l); mask++) {
                std::bitset<32> bi(mask);
                if (bi.count()%2 == 0) {
                    potentials[mask] = 1.0;
                } else {
                    potentials[mask] = 0;
                }
            }
            mrf->setChkNodePotential(n+i, potentials);
        }

        // Each edge has logNodePotential to be a 64 x 2 matrix
        mrf->createEdgeMask();
        for (uint64_t i=0; i<m; i++) {
            for (uint64_t j=0; j<l; j++) {
                mrf->addEdge(n+i, permutation.at(i*l+j)/k);
            }
        }
        return mrf;
    }
}