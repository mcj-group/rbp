/**
 * Created by vaksenov on 24.07.2019.
 */
#pragma once

#include <cstdint>

class MRF;

namespace examples_mrf {

MRF* isingMRF(uint64_t n, uint64_t m, uint64_t C, uint64_t seed);

static inline MRF* isingMRF(uint64_t n, uint64_t C, uint64_t seed) {
    return isingMRF(n, n, C, seed);
}

MRF* pottsMRF(uint64_t n, uint64_t C, uint64_t seed);

MRF* randomTree(uint64_t n, uint64_t C, uint64_t seed);

MRF* deterministicTree(uint64_t n);

} // namespace examples_mrf
