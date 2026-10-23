#pragma once
#include "level6_core.hpp"

// Dense masks for P and its halo only. Beam physics is cellwise and distant
// matter neither blocks rays nor influences captures, so this projection is
// exact. Keep the original Case for independent validation of generated paths.
static Case packed_relevant(const Case& original) {
    Case result = original;
    vector<int> cells;
    B relevant = bor(original.P, original.H);
    for (int i = 0; i < WD * 64; ++i) if ((relevant.w[i / 64] >> (i % 64)) & 1) cells.push_back(i);
    auto pack = [&](const B& source) {
        B target = bzero();
        for (size_t i = 0; i < cells.size(); ++i)
            if ((source.w[cells[i] / 64] >> (cells[i] % 64)) & 1) bset(target, i);
        return target;
    };
    result.P = pack(original.P); result.H = pack(original.H);
    for (int e = 0; e < 6; ++e) for (int p = 0; p < original.L; ++p) result.ray[e][p] = pack(original.ray[e][p]);
    return result;
}
