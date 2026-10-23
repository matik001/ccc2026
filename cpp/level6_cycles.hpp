// Closed schedules scored by missing target cells / dirty neighbours.
// Based on ViolModel from the colleague's e6d1c9c, with board-sized masks,
// explicit feasibility checks (no prefix<8 cutoff), and warm starts.
template<int NW> struct ClosedCycleModel {
    using BB = SmallB<NW>;
    const Case& cs;
    vector<int> segments;
    int period;
    BB target, halo;
    vector<BB> rays[6];
    vector<string> sequences;
    vector<array<vector<string>, 2>> paths;
    ClosedCycleModel(const Case& c, const vector<int>& segs)
        : cs(c), segments(segs), period(accumulate(segs.begin(), segs.end(), 0)),
          target(smallBits<NW>(c.P)), halo(smallBits<NW>(c.H)), sequences(all_sequences(period)) {
        for (int e = 0; e < 6; ++e)
            for (const auto& ray : cs.ray[e]) rays[e].push_back(smallBits<NW>(ray));
        if (period <= 6) {
            paths.resize(2 * period + 1);
            for (int code = 0; code < (1 << (2 * period)); ++code) {
                string ops(period, '-'); int value = code, net = 0, parity = 0;
                for (char& op : ops) {
                    op = "FBT-"[value & 3]; value >>= 2;
                    net += (op == 'F') - (op == 'B'); parity ^= op == 'T';
                }
                paths[net + period][parity].push_back(move(ops));
            }
        }
    }
    string random_seq(Rng& rng) const {
        return sequences.empty() ? random_sequence(period, rng) : sequences[rng.below(sequences.size())];
    }
    string path(Rng& rng, int net, int parity) const {
        if (abs(net) + parity > period) return {};
        if (!paths.empty()) {
            const auto& choices = paths[net + period][parity];
            return choices[rng.below(choices.size())];
        }
        int toggles = parity + 2 * rng.below((period - abs(net) - parity) / 2 + 1);
        int backtracks = rng.below((period - abs(net) - toggles) / 2 + 1);
        string ops(max(0, net) + backtracks, 'F');
        ops += string(max(0, -net) + backtracks, 'B'); ops += string(toggles, 'T');
        ops.resize(period, '-');
        for (int i = period - 1; i > 0; --i) swap(ops[i], ops[rng.below(i + 1)]);
        return ops;
    }
    int prefix(const int* positions, const int* active) const {
        int time = 0;
        for (int e = 0; e < 6; ++e)
            time = max(time, abs(cs.delta(cs.home(e), positions[e])) + active[e]);
        return time;
    }
    void masks(const int* pos, const int* active, BB& two, BB& three) const {
        BB one{}; two = {}; three = {};
        for (int e = 0; e < 6; ++e) if (active[e]) {
            const BB& ray = rays[e][pos[e]];
            for (int w = 0; w < NW; ++w) {
                u64 o = one.w[w], d = two.w[w], h = three.w[w], x = ray.w[w];
                three.w[w] = h | (d & x);
                two.w[w] = (d & ~x) | (o & x);
                one.w[w] = (o & ~x) | (x & ~(o | d | h));
            }
        }
    }
    int violations(const int* start, const int* enabled, const string* ops) const {
        int pos[6], active[6];
        copy(start, start + 6, pos); copy(enabled, enabled + 6, active);
        BB two{}, three{};
        masks(pos, active, two, three);
        BB alt = bandn(two, three);
        if (segments.size() > 1 && period <= 24) {
            // Select capture times with dynamic programming. Fixing every gap
            // beforehand can hide equally fast schedules with uneven gaps.
            BB create[24], erase[24];
            for (int phase = 0; phase < period; ++phase) {
                for (int e = 0; e < 6; ++e) {
                    char op = ops[e][phase];
                    if (op == 'T') active[e] ^= 1;
                    else if (op == 'F') { if (++pos[e] == cs.L) pos[e] = 0; }
                    else if (op == 'B') { if (--pos[e] < 0) pos[e] = cs.L - 1; }
                }
                masks(pos, active, create[phase], erase[phase]);
            }
            int cost[25][25]{};
            for (int first = 0; first < period; ++first) {
                BB matter{}, alternative = alt;
                for (int last = first + 1; last <= period; ++last) {
                    matter = bandn(bor(matter, create[last - 1]), erase[last - 1]);
                    if (first == 0) {
                        alternative = bandn(bor(alternative, create[last - 1]), erase[last - 1]);
                        cost[first][last] = pc(bandn(target, band(matter, alternative))) + pc(band(halo, bor(matter, alternative)));
                    } else cost[first][last] = pc(bandn(target, matter)) + pc(band(halo, matter));
                }
            }
            int dp[25][25];
            for (auto& row : dp) fill(begin(row), end(row), 1000000);
            dp[0][0] = 0;
            for (int group = 1; group <= static_cast<int>(segments.size()); ++group)
                for (int last = group; last <= period; ++last)
                    for (int first = group - 1; first < last; ++first)
                        dp[group][last] = min(dp[group][last], dp[group - 1][first] + cost[first][last]);
            return dp[segments.size()][period];
        }
        int bad = 0, phase = 0;
        for (size_t group = 0; group < segments.size(); ++group) {
            BB matter{};
            for (int t = 0; t < segments[group]; ++t, ++phase) {
                for (int e = 0; e < 6; ++e) {
                    char op = ops[e][phase];
                    if (op == 'T') active[e] ^= 1;
                    else if (op == 'F') { if (++pos[e] == cs.L) pos[e] = 0; }
                    else if (op == 'B') { if (--pos[e] < 0) pos[e] = cs.L - 1; }
                }
                masks(pos, active, two, three);
                matter = bandn(bor(matter, two), three);
                if (group == 0) alt = bandn(bor(alt, two), three);
            }
            if (group == 0)
                bad += pc(bandn(target, band(matter, alt))) + pc(band(halo, bor(matter, alt)));
            else bad += pc(bandn(target, matter)) + pc(band(halo, matter));
        }
        return bad;
    }
    Program program(const int* positions, const int* active, const string* ops, int limit, int swaps = 0) const {
        int setup = prefix(positions, active);
        if (setup > limit) return {};
        Program result;
        for (int t = 1; t <= setup; ++t) {
            string tick(6, '-');
            for (int e = 0; e < 6; ++e) {
                int d = cs.delta(cs.home(e), positions[e]);
                int first = setup - abs(d) - active[e] + 1;
                if (first <= t && t < first + abs(d)) tick[e] = d > 0 ? 'F' : 'B';
                else if (active[e] && t == setup) tick[e] = 'T';
            }
            result.push_back(tick);
        }
        for (int time = 0; result.size() < static_cast<size_t>(limit); ++time) {
            int phase = time % period;
            string tick(6, '-');
            for (int e = 0; e < 6; ++e) tick[e] = ops[e][phase];
            if ((time / period) % 2) for (int axis = 0; axis < 3; ++axis) if (swaps & (1 << axis)) {
                auto invert = [](char op) { return op == 'F' ? 'B' : op == 'B' ? 'F' : op; };
                char a = tick[axis], b = tick[axis + 3];
                tick[axis] = invert(b); tick[axis + 3] = invert(a);
            }
            result.push_back(tick);
        }
        if (!cs.finish(result)) return {};
        return result;
    }
};

template<int NW>
static Program closed_cycles_impl(const Case& cs, const Program& baseline,
        const vector<vector<int>>& specs, int iterations, int restarts, Rng& rng, bool allow_swaps) {
    Program best;
    int limit = baseline.size() - 1;
    struct Seed { Node start; string ops[6]; int swaps; int length; };
    vector<pair<int, Node>> captures{{0, state_after(cs, {})}};
    for (int t = 1; t <= static_cast<int>(baseline.size()); ++t) {
        Node state = state_after(cs, Program(baseline.begin(), baseline.begin() + t));
        if (state.acc > captures.back().second.acc) captures.push_back({t, state});
    }
    vector<Seed> seeds;
    for (size_t i = 0; i < captures.size(); ++i) for (size_t j = i + 1; j < captures.size() && j <= i + 4; ++j) {
        const Node& a = captures[i].second; const Node& b = captures[j].second;
        int swaps = 0; bool closed = true;
        for (int axis = 0; axis < 3; ++axis) {
            auto coordinate = [&](const Node& st, int e) {
                int offset = cs.delta(cs.home(e), st.pos[e]);
                return pair<int, int>{e < 3 ? offset : -offset, (st.act >> e) & 1};
            };
            if (coordinate(a, axis) == coordinate(b, axis) && coordinate(a, axis + 3) == coordinate(b, axis + 3)) continue;
            if (allow_swaps && coordinate(a, axis) == coordinate(b, axis + 3) && coordinate(a, axis + 3) == coordinate(b, axis)) swaps |= 1 << axis;
            else { closed = false; break; }
        }
        int length = captures[j].first - captures[i].first;
        if (!closed || length > 24) continue;
        Seed seed{a, {}, swaps, length};
        for (int t = captures[i].first; t < captures[j].first; ++t)
            for (int e = 0; e < 6; ++e) seed.ops[e] += baseline[t][e];
        seeds.push_back(move(seed));
    }
    for (const auto& segments : specs) {
        int k = accumulate(segments.begin(), segments.end(), 0);
        // Only periods with room for a useful number of captures.
        if (k * (cs.req / static_cast<int>(segments.size())) > limit + k) continue;
        ClosedCycleModel<NW> model(cs, segments);
        string idle(k, '-');
        vector<const Seed*> suitable;
        for (const auto& seed : seeds) if (seed.length >= k && seed.length <= k + 3) suitable.push_back(&seed);
        for (int restart = 0; restart < restarts; ++restart) {
            int positions[6], active[6];
            int swaps = 0;
            string ops[6];
            auto random_pos = [&](int e) {
                return (cs.home(e) + rng.below(2 * cs.n - 1) - (cs.n - 1) + cs.L) % cs.L;
            };
            Node warm{};
            const Seed* chosen = (!suitable.empty() && restart % 3 != 0) ? suitable[rng.below(suitable.size())] : nullptr;
            bool use_warm = chosen || restart % 3 == 1;
            if (use_warm) {
                if (chosen) warm = chosen->start;
                else {
                    int cut = rng.below(baseline.size());
                    warm = state_after(cs, Program(baseline.begin(), baseline.begin() + cut));
                }
            }
            for (int e = 0; e < 6; ++e) {
                positions[e] = use_warm ? warm.pos[e] : random_pos(e);
                active[e] = use_warm ? ((warm.act >> e) & 1) : (rng.uni() < 0.65);
            }
            auto required_net = [&](int axis) {
                if (!(swaps & (1 << axis))) return 0;
                int destination = (cs.home(axis) - cs.delta(cs.home(axis + 3), positions[axis + 3]) + cs.L) % cs.L;
                return cs.delta(positions[axis], destination);
            };
            auto parity = [&](int axis) { return (swaps & (1 << axis)) ? (active[axis] ^ active[axis + 3]) : 0; };
            auto regenerate = [&](int axis) {
                int net = required_net(axis), flips = parity(axis);
                if (abs(net) + flips > k) return false;
                for (int e : {axis, axis + 3}) ops[e] = model.path(rng, net, flips);
                return true;
            };
            for (int axis = 0; axis < 3; ++axis) {
                if ((chosen && (chosen->swaps & (1 << axis))) || (!chosen && allow_swaps && rng.uni() < 0.6)) swaps |= 1 << axis;
                if (!regenerate(axis)) { swaps &= ~(1 << axis); regenerate(axis); }
            }
            if (chosen) for (int e = 0; e < 6; ++e) {
                string proposed = chosen->ops[e];
                while (static_cast<int>(proposed.size()) > k) {
                    vector<int> idle_positions;
                    for (int i = 0; i < static_cast<int>(proposed.size()); ++i) if (proposed[i] == '-') idle_positions.push_back(i);
                    if (!idle_positions.empty()) { proposed.erase(proposed.begin() + idle_positions[rng.below(idle_positions.size())]); continue; }
                    if (static_cast<int>(proposed.size()) < k + 2) break;
                    vector<pair<int, int>> cancellations;
                    for (int i = 0; i < static_cast<int>(proposed.size()); ++i)
                        for (int j = i + 1; j < static_cast<int>(proposed.size()); ++j)
                            if ((proposed[i] == 'F' && proposed[j] == 'B') || (proposed[i] == 'B' && proposed[j] == 'F') ||
                                (proposed[i] == 'T' && proposed[j] == 'T')) cancellations.push_back({i, j});
                    if (cancellations.empty()) break;
                    auto [i, j] = cancellations[rng.below(cancellations.size())];
                    proposed.erase(proposed.begin() + j); proposed.erase(proposed.begin() + i);
                }
                int net = 0, flips = 0;
                for (char op : proposed) { net += (op == 'F') - (op == 'B'); flips ^= op == 'T'; }
                if (static_cast<int>(proposed.size()) == k && net == required_net(e % 3) && flips == parity(e % 3)) ops[e] = move(proposed);
            }
            int violations = model.violations(positions, active, ops);
            double value = 8.0 * violations + 0.3 * model.prefix(positions, active);
            double temp = 6.0, cooling = pow(0.2 / temp, 1.0 / max(1, iterations));
            for (int it = 0; it < iterations; ++it, temp *= cooling) {
                int e = rng.below(6), axis = e % 3, other = axis + 3, old_swaps = swaps;
                int old_pos[2] = {positions[axis], positions[other]};
                int old_active[2] = {active[axis], active[other]};
                string old_ops[2] = {ops[axis], ops[other]};
                double kind = rng.uni();
                bool possible = true;
                if (kind < 0.2) {
                    ops[e] = model.path(rng, required_net(axis), parity(axis));
                    possible = static_cast<int>(ops[e].size()) == k;
                }
                else if (kind < 0.32) swap(ops[e][rng.below(k)], ops[e][rng.below(k)]);
                else if (kind < 0.55) {
                    positions[e] = (positions[e] + (rng.below(2) ? 1 : cs.L - 1)) % cs.L;
                    if (swaps & (1 << axis)) possible = regenerate(axis);
                } else if (kind < 0.67) {
                    active[e] ^= 1;
                    if (swaps & (1 << axis)) possible = regenerate(axis);
                } else if (kind < 0.77) {
                    positions[e] = random_pos(e);
                    if (swaps & (1 << axis)) possible = regenerate(axis);
                } else if (kind < 0.85) {
                    if (allow_swaps) { swaps ^= 1 << axis; possible = regenerate(axis); }
                    else ops[e] = idle;
                }
                else {
                    char first[2] = {ops[axis][0], ops[other][0]};
                    for (int side = 0; side < 2; ++side) {
                        int emitter = axis + 3 * side; char op = first[side];
                        if (op == 'T') active[emitter] ^= 1;
                        else if (op == 'F') positions[emitter] = (positions[emitter] + 1) % cs.L;
                        else if (op == 'B') positions[emitter] = (positions[emitter] + cs.L - 1) % cs.L;
                        rotate(ops[emitter].begin(), ops[emitter].begin() + 1, ops[emitter].end());
                        if (swaps & (1 << axis)) {
                            char moved = first[1 - side];
                            ops[emitter].back() = moved == 'F' ? 'B' : moved == 'B' ? 'F' : moved;
                        }
                    }
                }
                int next_violations = possible ? model.violations(positions, active, ops) : INT_MAX / 16;
                double next = 8.0 * next_violations + 0.3 * model.prefix(positions, active);
                if (next <= value || rng.uni() < exp((value - next) / temp)) {
                    bool changed = next_violations != violations || next != value;
                    value = next; violations = next_violations;
                    if (violations == 0 && (changed || it % 128 == 0)) {
                        Program p = model.program(positions, active, ops, limit, swaps);
                        if (!p.empty()) { best = move(p); limit = best.size() - 1; }
                    }
                } else {
                    swaps = old_swaps;
                    for (int side = 0; side < 2; ++side) {
                        int emitter = axis + 3 * side;
                        positions[emitter] = old_pos[side]; active[emitter] = old_active[side];
                        ops[emitter] = move(old_ops[side]);
                    }
                }
            }
        }
    }
    return best;
}

static Program closed_cycles(const Case& cs, const Program& baseline,
        const vector<vector<int>>& specs, int iterations, int restarts, Rng& rng, bool allow_swaps = false) {
    switch ((3 * cs.n * (cs.n - 1) + 64) / 64) {
        case 1: return closed_cycles_impl<1>(cs, baseline, specs, iterations, restarts, rng, allow_swaps);
        case 2: return closed_cycles_impl<2>(cs, baseline, specs, iterations, restarts, rng, allow_swaps);
        case 3: return closed_cycles_impl<3>(cs, baseline, specs, iterations, restarts, rng, allow_swaps);
        case 4: return closed_cycles_impl<4>(cs, baseline, specs, iterations, restarts, rng, allow_swaps);
        case 5: return closed_cycles_impl<5>(cs, baseline, specs, iterations, restarts, rng, allow_swaps);
        case 6: return closed_cycles_impl<6>(cs, baseline, specs, iterations, restarts, rng, allow_swaps);
        default: return closed_cycles_impl<7>(cs, baseline, specs, iterations, restarts, rng, allow_swaps);
    }
}
