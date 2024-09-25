/**
 * Created by vaksenov on 25.07.2019.
 * Ported to C++ by Mark Jeffrey 2021.04.26
 */
#pragma once

#include <array>
#include <cstdint>

class Edge {
  public:
    static constexpr uint64_t LENGTH = 2;
    using array2d_t = std::array<std::array<double,LENGTH>,LENGTH>;

  private:

    uint64_t u, v;
    array2d_t logPotentials;
    array2d_t potentials;

  public:

    Edge(uint64_t u, uint64_t v, array2d_t _potentials);

    double getPotential(uint64_t i, uint64_t, uint64_t vi, uint64_t vj) const {
        if (u == i) {
            return potentials[vi][vj];
        } else {
            return potentials[vj][vi];
        }
    }

    double getLogPotential(uint64_t i, uint64_t,
                           uint64_t vi, uint64_t vj) const {
        if (u == i) {
            return logPotentials[vi][vj];
        } else {
            return logPotentials[vj][vi];
        }
    }
};
