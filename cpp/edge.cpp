#include <cmath>

#include "edge.h"

Edge::Edge(uint64_t u, uint64_t v, array2d_t _potentials)
  : u(u), v(v), potentials(_potentials) {
    for (uint64_t i = 0; i < LENGTH; i++) {
        for (uint64_t j = 0; j < LENGTH; j++) {
            logPotentials[i][j] = std::log(potentials[i][j]);
        }
    }
}
