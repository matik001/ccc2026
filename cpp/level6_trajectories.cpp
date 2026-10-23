#include "level6_packed.hpp"

struct Boundary { array<int, 6> pos{}; int active = 0; };

// Large-neighbourhood search over complete emitter trajectories. Two to four
// timelines are fixed; the remaining paths are rebuilt after compressing time.
// Backward reachability masks safely prune matter that can no longer be fixed.
template<int W> class TrajectorySearch {
    using Bits = SmallB<W>;
    struct Move { int state; char op; };
    struct Physics { Bits two, three, one; };
    struct FixedFuture { Bits zero{}, one{}, two{}, three{}; };
    struct Node {
        Bits matter{};
        int a = 0, b = 0, c = 0, d = 0, captures = 0, parent = -1;
        char op_a = '-', op_b = '-', op_c = '-', op_d = '-';
        double score = 0;
    };
    const Case& cs;
    int emit_a, emit_b, emit_c, emit_d, count, width, length;
    double alpha, potential;
    Bits target, halo, relevant, initial_matter{}, final_matter{};
    vector<Bits> rays[4], single_future[4];
    vector<FixedFuture> fixed_future;
    vector<vector<Move>> moves;
    vector<Bits> fixed_one, fixed_two, fixed_three, fixed_any;
    vector<Physics> reachable;
    Program skeleton;
    Rng rng;
    Boundary start, finish;
    bool has_end = false, custom_start = false, valid = true;
    int state_code(const Boundary& boundary, int emitter) const {
        int offset = cs.delta(cs.home(emitter), boundary.pos[emitter]);
        return 2 * (offset + cs.n) + ((boundary.active >> emitter) & 1);
    }
    Physics config(int tick, int a, int b) const {
        const Bits& x = rays[0][a]; const Bits& y = rays[1][b];
        Bits any = bor(x, y), both = band(x, y);
        return {
            bor(bor(bandn(fixed_two[tick], any), band(fixed_one[tick], bxor(x, y))), bandn(both, fixed_any[tick])),
            bor(bor(fixed_three[tick], band(fixed_two[tick], any)), band(fixed_one[tick], both)),
            bor(bandn(fixed_one[tick], any), bandn(bxor(x, y), fixed_any[tick]))
        };
    }
    const Physics& future(int tick, int a, int b) const { return reachable[(tick * count + a) * count + b]; }
    Physics triple_future(int tick, int a, int b, int c, int d) const {
        int remaining = length - tick;
        const Bits& x = single_future[0][remaining * count + a];
        const Bits& y = single_future[1][remaining * count + b];
        const Bits& z = single_future[2][remaining * count + c];
        Bits one = bor(bor(x, y), z), two = bor(bor(band(x, y), band(x, z)), band(y, z)), three = band(band(x, y), z);
        if (emit_d >= 0) {
            const auto& extra = single_future[3][remaining * count + d];
            three = bor(three, band(two, extra));
            two = bor(two, band(one, extra)); one = bor(one, extra);
        }
        const auto& f = fixed_future[tick];
        return {bor(bor(band(f.zero, two), band(f.one, one)), f.two),
                bor(bor(bor(f.three, band(f.two, one)), band(f.one, two)), band(f.zero, three)), {}};
    }
    u64 hash(const Node& node) const {
        u64 h = node.a + 257ULL * node.b + 65537ULL * node.c + 16777259ULL * node.d + 4294967311ULL * node.captures;
        for (u64 word : node.matter.w) { h ^= word + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); }
        return h;
    }
public:
    TrajectorySearch(const Case& c, Program fixed, int a, int b, int beam_width,
               double halo_weight, double potential_weight, u64 seed, int third = -1,
               const Boundary* begin = nullptr, const Boundary* end = nullptr, int fourth = -1,
               const B* begin_matter = nullptr, const B* end_matter = nullptr)
        : cs(c), emit_a(a), emit_b(b), emit_c(third), emit_d(fourth), count(2 * (2 * c.n + 1)), width(beam_width), length(fixed.size()),
          alpha(halo_weight), potential(potential_weight), target(smallBits<W>(c.P)), halo(smallBits<W>(c.H)),
          relevant(bor(target, halo)), moves(count), skeleton(move(fixed)), rng(seed) {
        for (int e = 0; e < 6; ++e) start.pos[e] = cs.home(e);
        if (begin) { start = *begin; custom_start = true; }
        if (end) { finish = *end; has_end = true; }
        if (begin_matter) initial_matter = band(smallBits<W>(*begin_matter), relevant);
        if (end_matter) final_matter = band(smallBits<W>(*end_matter), relevant);
        for (int e : {emit_a, emit_b, emit_c, emit_d}) if (e >= 0) {
            if (state_code(start, e) < 0 || state_code(start, e) >= count ||
                (has_end && (state_code(finish, e) < 0 || state_code(finish, e) >= count))) { valid = false; return; }
        }
        for (int state = 0; state < count; ++state) {
            int offset = state / 2 - cs.n, active = state % 2;
            moves[state].push_back({state, '-'});
            if (offset < cs.n) moves[state].push_back({state + 2, 'F'});
            if (offset > -cs.n) moves[state].push_back({state - 2, 'B'});
            moves[state].push_back({state ^ 1, 'T'});
            for (int k = 0; k < (emit_d >= 0 ? 4 : emit_c < 0 ? 2 : 3); ++k) {
                int emitter = k == 0 ? emit_a : k == 1 ? emit_b : k == 2 ? emit_c : emit_d;
                int raw = (cs.home(emitter) + offset + cs.L) % cs.L;
                rays[k].push_back(active ? smallBits<W>(cs.ray[emitter][raw]) : Bits{});
            }
        }
        int pos[6], active = start.active;
        copy(start.pos.begin(), start.pos.end(), pos);
        for (const auto& tick : skeleton) {
            Bits one{}, two{}, three{};
            for (int e = 0; e < 6; ++e) {
                if (e == emit_a || e == emit_b || e == emit_c || e == emit_d) continue;
                if (tick[e] == 'F') pos[e] = (pos[e] + 1) % cs.L;
                else if (tick[e] == 'B') pos[e] = (pos[e] + cs.L - 1) % cs.L;
                else if (tick[e] == 'T') active ^= 1 << e;
                if (!(active & (1 << e))) continue;
                Bits ray = smallBits<W>(cs.ray[e][pos[e]]);
                for (int w = 0; w < W; ++w) {
                    u64 o = one.w[w], d = two.w[w], h = three.w[w], x = ray.w[w];
                    three.w[w] = h | (d & x);
                    two.w[w] = (d & ~x) | (o & x);
                    one.w[w] = (o & ~x) | (x & ~(o | d | h));
                }
            }
            fixed_one.push_back(one); fixed_two.push_back(two); fixed_three.push_back(three);
            fixed_any.push_back(bor(bor(one, two), three));
        }
        if (has_end) for (int e = 0; e < 6; ++e) if (e != emit_a && e != emit_b && e != emit_c && e != emit_d) {
            if (pos[e] != finish.pos[e] || ((active >> e) & 1) != ((finish.active >> e) & 1)) { valid = false; return; }
        }
        if (emit_c >= 0) {
            fixed_future.resize(length + 1);
            for (int t = length - 1; t >= 0; --t) {
                const auto& later = fixed_future[t + 1];
                fixed_future[t] = {bor(later.zero, bandn(relevant, fixed_any[t])), bor(later.one, fixed_one[t]),
                                   bor(later.two, fixed_two[t]), bor(later.three, fixed_three[t])};
            }
            for (int k = 0; k < (emit_d < 0 ? 3 : 4); ++k) {
                single_future[k].resize((length + 1) * count);
                copy(rays[k].begin(), rays[k].end(), single_future[k].begin());
                for (int steps = 1; steps <= length; ++steps) for (int state = 0; state < count; ++state) {
                    Bits all{};
                    for (auto move : moves[state]) all = bor(all, single_future[k][(steps - 1) * count + move.state]);
                    single_future[k][steps * count + state] = all;
                }
            }
            return;
        }
        reachable.resize((length + 1) * count * count);
        // Each mask is an optimistic union over every legal future path.
        // A missing target needs some future creation; a dirty halo needs an
        // erasure. Their absence proves a branch cannot complete the task.
        vector<Physics> at(count * count), first(count * count);
        for (int t = length - 1; t >= 0; --t) {
            for (int a = 0; a < count; ++a) for (int b = 0; b < count; ++b) {
                auto now = config(t, a, b); const auto& later = future(t + 1, a, b);
                at[a * count + b] = {band(bor(now.two, later.two), relevant), band(bor(now.three, later.three), relevant)};
            }
            for (int a = 0; a < count; ++a) for (int b = 0; b < count; ++b) {
                Physics value{};
                for (auto move : moves[a]) {
                    const auto& p = at[move.state * count + b];
                    value.two = bor(value.two, p.two); value.three = bor(value.three, p.three);
                }
                first[a * count + b] = value;
            }
            for (int a = 0; a < count; ++a) for (int b = 0; b < count; ++b) {
                Physics value{};
                for (auto move : moves[b]) {
                    const auto& p = first[a * count + move.state];
                    value.two = bor(value.two, p.two); value.three = bor(value.three, p.three);
                }
                reachable[(t * count + a) * count + b] = value;
            }
        }
    }
    Program run() {
        if (!valid) return {};
        int first_a = state_code(start, emit_a), first_b = state_code(start, emit_b);
        int first_c = emit_c < 0 ? 2 * cs.n : state_code(start, emit_c);
        int first_d = emit_d < 0 ? 2 * cs.n : state_code(start, emit_d);
        int end_a = has_end ? state_code(finish, emit_a) : 0, end_b = has_end ? state_code(finish, emit_b) : 0;
        int end_c = has_end && emit_c >= 0 ? state_code(finish, emit_c) : 0;
        int end_d = has_end && emit_d >= 0 ? state_code(finish, emit_d) : 0;
        auto possible = emit_c < 0 ? future(0, first_a, first_b) : triple_future(0, first_a, first_b, first_c, first_d);
        Bits initial_missing = bandn(cs.req > 0 ? target : final_matter, initial_matter);
        if (!bnone(bandn(initial_missing, possible.two))) return {};
        Node initial; initial.a = first_a; initial.b = first_b; initial.c = first_c; initial.d = first_d;
        initial.matter = initial_matter;
        const vector<Move> idle{{first_c, '-'}};
        const vector<Move> fourth_idle{{first_d, '-'}};
        vector<vector<Node>> layers{{initial}};
        for (int t = 0; t < length; ++t) {
            vector<Node> children;
            children.reserve(layers.back().size() * (emit_d >= 0 ? 256 : emit_c < 0 ? 16 : 64));
            for (int parent = 0; parent < static_cast<int>(layers.back().size()); ++parent) {
                const auto& old = layers.back()[parent];
                for (auto a : moves[old.a]) for (auto b : moves[old.b]) {
                    auto pair_physics = config(t, a.state, b.state);
                    for (auto c : (emit_c < 0 ? idle : moves[old.c])) {
                    auto three_physics = pair_physics;
                    if (emit_c >= 0) {
                        const auto& ray = rays[2][c.state];
                        three_physics.two = bor(bandn(pair_physics.two, ray), band(pair_physics.one, ray));
                        three_physics.three = bor(pair_physics.three, band(pair_physics.two, ray));
                        three_physics.one = bor(bandn(pair_physics.one, ray), bandn(ray, bor(bor(pair_physics.one, pair_physics.two), pair_physics.three)));
                    }
                    for (auto d : (emit_d < 0 ? fourth_idle : moves[old.d])) {
                    auto phys = three_physics;
                    if (emit_d >= 0) {
                        const auto& ray = rays[3][d.state];
                        phys.two = bor(bandn(three_physics.two, ray), band(three_physics.one, ray));
                        phys.three = bor(three_physics.three, band(three_physics.two, ray));
                    }
                    Node next;
                    next.a = a.state; next.b = b.state; next.c = c.state; next.d = d.state; next.parent = parent;
                    next.op_a = a.op; next.op_b = b.op; next.op_c = c.op; next.op_d = d.op; next.captures = old.captures;
                    next.matter = band(bandn(bor(old.matter, phys.two), phys.three), relevant);
                    if (beq(band(next.matter, target), target) && bnone(band(next.matter, halo))) {
                        ++next.captures; next.matter = bandn(next.matter, target);
                    }
                    bool endpoint = !has_end || (t == length - 1 && a.state == end_a && b.state == end_b &&
                                                 (emit_c < 0 || c.state == end_c) && (emit_d < 0 || d.state == end_d) && beq(next.matter, final_matter));
                    if (next.captures >= cs.req && endpoint) {
                            Program result(skeleton.begin(), skeleton.begin() + t + 1);
                            result[t][emit_a] = a.op; result[t][emit_b] = b.op;
                            if (emit_c >= 0) result[t][emit_c] = c.op;
                            if (emit_d >= 0) result[t][emit_d] = d.op;
                            int index = parent;
                            for (int j = t - 1; j >= 0; --j) {
                                const auto& node = layers[j + 1][index];
                                result[j][emit_a] = node.op_a; result[j][emit_b] = node.op_b;
                                if (emit_c >= 0) result[j][emit_c] = node.op_c;
                                if (emit_d >= 0) result[j][emit_d] = node.op_d;
                                index = node.parent;
                            }
                            if (!custom_start && !cs.finish(result)) throw runtime_error("Trajectory reconstruction failed simulation");
                            return result;
                    }
                    if (cs.req - next.captures > length - t - 1) continue;
                    if (has_end) {
                        auto distance = [](int from, int to) { return abs(from / 2 - to / 2) + ((from & 1) != (to & 1)); };
                        int remaining_steps = length - t - 1;
                        if (distance(a.state, end_a) > remaining_steps || distance(b.state, end_b) > remaining_steps ||
                            (emit_c >= 0 && distance(c.state, end_c) > remaining_steps) ||
                            (emit_d >= 0 && distance(d.state, end_d) > remaining_steps)) continue;
                    }
                    auto remaining = emit_c < 0 ? future(t + 1, a.state, b.state) : triple_future(t + 1, a.state, b.state, c.state, d.state);
                    bool captured = next.captures >= cs.req;
                    Bits missing = bandn(captured && has_end ? final_matter : target, next.matter);
                    Bits dirty = captured && has_end ? bandn(next.matter, final_matter) : band(halo, next.matter);
                    if ((!bnone(bandn(missing, remaining.two)) && (!captured || has_end)) || !bnone(bandn(dirty, remaining.three))) continue;
                    double pot = 0;
                    for (int k = 0; k < (emit_d >= 0 ? 4 : emit_c < 0 ? 2 : 3); ++k) {
                        int state = k == 0 ? a.state : k == 1 ? b.state : k == 2 ? c.state : d.state;
                        const auto& ray = rays[k][state | 1];
                        pot += pc(band(ray, missing)) + 2 * pc(band(ray, dirty));
                    }
                    double progress = captured && has_end ? -4.0 * pc(missing) : 4.0 * pc(band(target, next.matter));
                    next.score = 10000.0 * min(next.captures, cs.req) + progress -
                                 4.0 * alpha * pc(dirty) + potential * pot + 0.8 * rng.uni();
                    children.push_back(next);
                    }
                    }
                }
            }
            if (children.empty()) return {};
            vector<Node> next;
            auto better = [](const Node& a, const Node& b) { return a.score > b.score; };
            size_t cutoff = min(children.size(), static_cast<size_t>(width) * 4);
            for (;;) {
                if (cutoff < children.size()) nth_element(children.begin(), children.begin() + cutoff, children.end(), better);
                sort(children.begin(), children.begin() + cutoff, better);
                unordered_set<u64> seen;
                next.clear();
                for (size_t i = 0; i < cutoff; ++i) {
                    const auto& child = children[i];
                    if (!seen.insert(hash(child)).second) continue;
                    next.push_back(child);
                    if (static_cast<int>(next.size()) == width) break;
                }
                if (static_cast<int>(next.size()) == width || cutoff == children.size()) break;
                cutoff = min(children.size(), cutoff * 2);
            }
            layers.push_back(move(next));
        }
        return {};
    }
};

static Program compress(const Program& program, int drop, Rng& rng) {
    int n = program.size(), cut = rng.below(n);
    Program result(n - drop, "------");
    for (int e = 0; e < 6; ++e) {
        string column;
        for (const auto& tick : program) column += tick[e];
        for (int k = 0; k < drop; ++k) {
            vector<int> idle;
            for (int i = 0; i < static_cast<int>(column.size()); ++i)
                if (column[i] == '-' && abs(i - cut) < 8) idle.push_back(i);
            if (idle.empty()) for (int i = 0; i < static_cast<int>(column.size()); ++i) if (column[i] == '-') idle.push_back(i);
            if (idle.empty()) {
                vector<pair<int, int>> cancellations;
                for (int a = 0; a < static_cast<int>(column.size()); ++a)
                    for (int b = a + 1; b < min(a + 7, static_cast<int>(column.size())); ++b)
                        if ((column[a] == 'F' && column[b] == 'B') || (column[a] == 'B' && column[b] == 'F') ||
                            (column[a] == 'T' && column[b] == 'T')) cancellations.push_back({a, b});
                if (!cancellations.empty()) {
                    auto [a, b] = cancellations[rng.below(cancellations.size())];
                    column.erase(column.begin() + b); column.erase(column.begin() + a);
                    column.insert(column.begin() + min(a, static_cast<int>(column.size())), '-');
                    continue;
                }
            }
            int remove = idle.empty() ? min(cut, static_cast<int>(column.size()) - 1) : idle[rng.below(idle.size())];
            column.erase(column.begin() + remove);
        }
        for (int t = 0; t < n - drop; ++t) result[t][e] = column[t];
    }
    return result;
}
template<int W> static Program optimize_segments(const Case& cs, Program best, int trials, int width,
                                                int max_drop, int freed, Rng& rng) {
    const size_t before = best.size();
    Case segment_case = cs; segment_case.req = 1;
    map<string, Program> cache;
    int spent = 0;
    bool improved;
    do {
        improved = false;
        vector<pair<int, Boundary>> boundaries;
        Boundary state;
        for (int e = 0; e < 6; ++e) state.pos[e] = cs.home(e);
        boundaries.push_back({0, state});
        B matter = bzero();
        for (int t = 0; t < static_cast<int>(best.size()); ++t) {
            for (int e = 0; e < 6; ++e) {
                if (best[t][e] == 'F') state.pos[e] = (state.pos[e] + 1) % cs.L;
                else if (best[t][e] == 'B') state.pos[e] = (state.pos[e] + cs.L - 1) % cs.L;
                else if (best[t][e] == 'T') state.active ^= 1 << e;
            }
            B two, three; cs.config(state.pos.data(), state.active, two, three);
            matter = bandn(bor(matter, two), three);
            if (beq(band(matter, cs.P), cs.P) && bnone(band(matter, cs.H))) {
                matter = bandn(matter, cs.P); boundaries.push_back({t + 1, state});
            }
        }
        vector<int> order(boundaries.size() - 1);
        iota(order.begin(), order.end(), 0);
        sort(order.begin(), order.end(), [&](int a, int b) {
            return boundaries[a + 1].first - boundaries[a].first > boundaries[b + 1].first - boundaries[b].first;
        });
        for (int index : order) {
            auto [begin_tick, begin] = boundaries[index];
            auto [end_tick, end] = boundaries[index + 1];
            bool last = index + 2 == static_cast<int>(boundaries.size());
            Program source(best.begin() + begin_tick, best.begin() + end_tick);
            if (source.size() < 2) continue;
            string key = last ? "free" : "fixed";
            key += ":" + to_string(begin.active) + ":" + to_string(end.active);
            for (int e = 0; e < 6; ++e) key += ":" + to_string(begin.pos[e]) + ":" + to_string(end.pos[e]);
            for (const auto& tick : source) key += tick;
            auto found = cache.find(key);
            if (found == cache.end()) {
                if (spent >= trials) continue;
                Program replacement;
                int budget = max(4, trials / max(1, static_cast<int>(order.size())));
                for (int trial = 0; trial < budget && spent < trials; ++trial, ++spent) {
                    int a = rng.below(6), b, c = -1, d = -1;
                    do { b = rng.below(6); } while (b == a);
                    if (freed >= 3) do { c = rng.below(6); } while (c == a || c == b);
                    if (freed >= 4) do { d = rng.below(6); } while (d == a || d == b || d == c);
                    int drop = 1 + (trial % 4 == 3 ? rng.below(min(max_drop, static_cast<int>(source.size()) - 1)) : 0);
                    auto draft = compress(source, drop, rng);
                    double alpha = trial % 3 == 0 ? 0.5 : trial % 3 == 1 ? 2 : 6;
                    TrajectorySearch<W> search(segment_case, move(draft), a, b, width, alpha, 0.4, rng.next(), c, &begin, last ? nullptr : &end, d);
                    replacement = search.run();
                    if (!replacement.empty()) { ++spent; break; }
                }
                found = cache.emplace(key, move(replacement)).first;
            }
            if (found->second.empty()) continue;
            Program candidate(best.begin(), best.begin() + begin_tick);
            candidate.insert(candidate.end(), found->second.begin(), found->second.end());
            candidate.insert(candidate.end(), best.begin() + end_tick, best.end());
            if (!cs.finish(candidate)) throw runtime_error("Segment replacement failed full simulation");
            if (candidate.size() < best.size()) { best = move(candidate); improved = true; break; }
        }
    } while (improved);
    return best.size() < before ? best : Program{};
}
static Program stretch(const Program& program, int extra, Rng& rng) {
    Program result(program.size() + extra, "------");
    int cut = rng.below(program.size() + 1);
    for (int e = 0; e < 6; ++e) {
        string column;
        for (const auto& tick : program) column += tick[e];
        for (int i = 0; i < extra; ++i) {
            int at = clamp(cut + rng.below(9) - 4, 0, static_cast<int>(column.size()));
            column.insert(column.begin() + at, '-');
        }
        for (size_t t = 0; t < result.size(); ++t) result[t][e] = column[t];
    }
    return result;
}
// A window may cross any number of captures, or none. Matching all relevant
// matter and emitter states makes its unchanged suffix valid by construction.
template<int W> static Program optimize_windows(const Case& cs, Program best, int trials, int width,
                                                int max_drop, int freed, int window, bool explore, Rng& rng) {
    struct Snapshot { Boundary state; B matter = bzero(); int captures = 0; };
    const size_t before = best.size();
    Program current = best;
    vector<Snapshot> snapshots;
    auto record = [&]() {
        snapshots.assign(1, {});
        for (int e = 0; e < 6; ++e) snapshots[0].state.pos[e] = cs.home(e);
        for (const auto& tick : current) {
            auto next = snapshots.back();
            for (int e = 0; e < 6; ++e) {
                if (tick[e] == 'F') next.state.pos[e] = (next.state.pos[e] + 1) % cs.L;
                else if (tick[e] == 'B') next.state.pos[e] = (next.state.pos[e] + cs.L - 1) % cs.L;
                else if (tick[e] == 'T') next.state.active ^= 1 << e;
            }
            B two, three; cs.config(next.state.pos.data(), next.state.active, two, three);
            next.matter = band(bandn(bor(next.matter, two), three), bor(cs.P, cs.H));
            if (beq(band(next.matter, cs.P), cs.P) && bnone(band(next.matter, cs.H))) {
                ++next.captures; next.matter = bandn(next.matter, cs.P);
            }
            snapshots.push_back(next);
        }
    };
    record();
    for (int trial = 0; trial < trials && best.size() > 1; ++trial) {
        if (explore && trial % 32 == 0) { current = best; record(); }
        int end = trial % 4 == 0 ? current.size() : 2 + rng.below(current.size() - 1);
        int span = min(end, min(window, 6 + rng.below(max(1, window - 5))));
        int begin = end - span;
        int drop = explore && trial % 5 == 4 ? 0 : 1 + (trial % 4 == 3 ? rng.below(min(max_drop, span - 1)) : 0);
        Program source(current.begin() + begin, current.begin() + end);
        Program draft = drop ? compress(source, drop, rng) : source;
        int a = rng.below(6), b, c = -1, d = -1;
        do { b = rng.below(6); } while (b == a);
        if (freed >= 3) do { c = rng.below(6); } while (c == a || c == b);
        if (freed >= 4) do { d = rng.below(6); } while (d == a || d == b || d == c);
        Case section = cs; section.req = snapshots[end].captures - snapshots[begin].captures;
        bool last = end == static_cast<int>(current.size());
        double alpha = trial % 3 == 0 ? 0.5 : trial % 3 == 1 ? 2 : 6;
        TrajectorySearch<W> search(section, move(draft), a, b, width, alpha, 0.4, rng.next(), c,
            &snapshots[begin].state, last ? nullptr : &snapshots[end].state, d,
            &snapshots[begin].matter, last ? nullptr : &snapshots[end].matter);
        auto replacement = search.run();
        if (replacement.empty()) continue;
        Program candidate(current.begin(), current.begin() + begin);
        candidate.insert(candidate.end(), replacement.begin(), replacement.end());
        candidate.insert(candidate.end(), current.begin() + end, current.end());
        if (!cs.finish(candidate)) throw runtime_error("Window replacement failed full simulation");
        if (candidate.size() < best.size()) best = candidate;
        if (candidate.size() <= current.size()) { current = move(candidate); record(); }
    }
    return best.size() < before ? best : Program{};
}
template<int W> static Program optimize(const Case& cs, Program best, int trials, int width, int max_drop,
                                       int freed, bool explore, Rng& rng, const vector<Program>& starts) {
    const size_t before = best.size();
    Program current = best;
    unordered_set<u64> visited;
    auto signature = [](const Program& program) {
        u64 value = 1469598103934665603ULL;
        for (const auto& tick : program) for (char c : tick) { value ^= c; value *= 1099511628211ULL; }
        return value;
    };
    visited.insert(signature(best));
    vector<pair<int, int>> pairs;
    for (int a = 0; a < 6; ++a) for (int b = a + 1; b < 6; ++b) pairs.push_back({a, b});
    for (int trial = 0; trial < trials && best.size() > static_cast<size_t>(cs.req); ++trial) {
        auto [a, b] = pairs[(trial + rng.below(pairs.size())) % pairs.size()];
        int drop = 1 + (trial % 4 == 3 ? rng.below(min(max_drop, static_cast<int>(best.size()) - cs.req)) : 0);
        if (!explore || trial % 20 == 0) current = best;
        if (explore && !starts.empty() && trial % 20 == 0) current = starts[rng.below(starts.size())];
        int length = best.size() - drop;
        if (explore && !starts.empty() && current.size() > best.size() && trial % 3 != 0) length = current.size() - drop;
        if (explore && trial % 6 >= 4) length = best.size() + (trial % 12 == 5);
        Program draft = static_cast<int>(current.size()) > length ? compress(current, current.size() - length, rng) :
                        static_cast<int>(current.size()) < length ? stretch(current, length - current.size(), rng) : current;
        if (cs.finish(draft) && draft.size() < best.size()) { best = draft; current = move(draft); continue; }
        double alpha = trial % 3 == 0 ? 0.5 : trial % 3 == 1 ? 2 : 6;
        int c = -1, d = -1;
        if (freed >= 3) { do { c = rng.below(6); } while (c == a || c == b); }
        if (freed >= 4) { do { d = rng.below(6); } while (d == a || d == b || d == c); }
        TrajectorySearch<W> search(cs, move(draft), a, b, width, alpha, 0.4, rng.next(), c, nullptr, nullptr, d);
        auto candidate = search.run();
        if (candidate.empty()) continue;
        if (candidate.size() < best.size()) best = candidate;
        if (explore && candidate.size() <= (starts.empty() ? best.size() + 2 : current.size() + 1) &&
            visited.insert(signature(candidate)).second) current = move(candidate);
    }
    return best.size() < before ? best : Program{};
}
static vector<string> split(const string& text, char separator = ',') {
    vector<string> out; stringstream stream(text); string value;
    while (getline(stream, value, separator)) if (!value.empty()) out.push_back(value);
    return out;
}
int main(int argc, char** argv) {
    map<string, string> options{{"names", "3-large,4-enormous"}, {"baseline", "cpp/outputs"},
        {"outdir", "cpp/candidates/trajectories"}, {"threads", "3"}, {"width", "200"},
        {"trials", "40"}, {"drop", "3"}, {"freed", "2"}, {"seed", "1"}, {"cases", ""}, {"mode", "whole"}, {"explore", "0"}, {"minlen", "0"}, {"window", "24"}, {"portfolio", "0"}};
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i]; auto equal = arg.find('=');
        if (equal == string::npos || !options.count(arg.substr(0, equal))) { cerr << "Unknown option: " << arg << '\n'; return 1; }
        options[arg.substr(0, equal)] = arg.substr(equal + 1);
    }
    int width = stoi(options["width"]), trials = stoi(options["trials"]), workers = stoi(options["threads"]), drop = stoi(options["drop"]);
    if (min({width, trials, workers, drop}) < 1) return 1;
    int freed = stoi(options["freed"]);
    if (freed < 2 || freed > 4) return 1;
    if (options["mode"] != "whole" && options["mode"] != "segments" && options["mode"] != "windows") return 1;
    int window = stoi(options["window"]);
    if (window < 2) return 1;
    int min_length = stoi(options["minlen"]);
    if (min_length < 0) return 1;
    int low = 0, high = INT_MAX;
    set<int> selected;
    if (options["cases"].find(',') != string::npos) for (const auto& value : split(options["cases"])) selected.insert(stoi(value));
    else if (!options["cases"].empty()) { auto limits = split(options["cases"], '-'); low = stoi(limits[0]); high = limits.size() > 1 ? stoi(limits[1]) : low; }
    for (const auto& name : split(options["names"])) {
        auto original_cases = load_cases("level-6/in_level-6_" + name + ".txt");
        vector<Case> cases;
        for (const auto& cs : original_cases) cases.push_back(packed_relevant(cs));
        string filename = "out_level-6_" + name + ".txt";
        auto baseline = read_programs(options["baseline"] + "/" + filename, cases.size());
        for (size_t i = 0; i < cases.size(); ++i) if (!cases[i].finish(baseline[i])) return 2;
        vector<vector<Program>> starts(cases.size());
        if (stoi(options["portfolio"]) != 0) {
            vector<string> paths{"cpp/outputs", "python/outputs"};
            for (const string parent : {"cpp/candidates", "python/candidates", "cpp/imported_candidates", "cpp"})
                if (filesystem::exists(parent)) for (const auto& child : filesystem::directory_iterator(parent))
                    if (child.is_directory() && (parent != "cpp" || child.path().filename().string().starts_with("baseline_")))
                        paths.push_back(child.path().string());
            sort(paths.begin(), paths.end());
            vector<unordered_set<string>> seen(cases.size());
            for (const auto& path : paths) if (filesystem::exists(path + "/" + filename)) {
                auto programs = read_programs(path + "/" + filename, cases.size());
                for (size_t i = 0; i < cases.size(); ++i) {
                    auto& program = programs[i];
                    if (program.empty() || program.size() > baseline[i].size() + max(size_t(6), baseline[i].size() / 6)) continue;
                    if (!cases[i].finish(program)) continue;
                    string key; for (const auto& tick : program) key += tick;
                    if (seen[i].insert(move(key)).second) starts[i].push_back(move(program));
                }
            }
            for (auto& pool : starts) {
                stable_sort(pool.begin(), pool.end(), [](const Program& a, const Program& b) { return a.size() < b.size(); });
                if (pool.size() > 32) pool.resize(32);
            }
        }
        vector<Program> result(cases.size()); atomic<int> next(0), done(0); mutex output_mutex;
        auto began = chrono::steady_clock::now();
        auto worker = [&]() {
            for (;;) {
                int i = next++; if (i >= static_cast<int>(cases.size())) break;
                if (i < low || i > high || (!selected.empty() && !selected.count(i)) || static_cast<int>(baseline[i].size()) < min_length) continue;
                const auto& cs = cases[i]; Rng rng(stoull(options["seed"]) * 1009 + i * 31);
                Program candidate;
                auto run = [&]<int W>() {
                    if (options["mode"] == "windows") return optimize_windows<W>(cs, baseline[i], trials, width, drop, freed,
                        window, stoi(options["explore"]) != 0, rng);
                    return options["mode"] == "segments" ? optimize_segments<W>(cs, baseline[i], trials, width, drop, freed, rng) :
                                                          optimize<W>(cs, baseline[i], trials, width, drop, freed, stoi(options["explore"]) != 0, rng, starts[i]);
                };
                switch ((pc(bor(cs.P, cs.H)) + 63) / 64) {
                    case 1: candidate = run.template operator()<1>(); break;
                    case 2: candidate = run.template operator()<2>(); break;
                    case 3: candidate = run.template operator()<3>(); break;
                    case 4: candidate = run.template operator()<4>(); break;
                    case 5: candidate = run.template operator()<5>(); break;
                    case 6: candidate = run.template operator()<6>(); break;
                    default: candidate = run.template operator()<7>();
                }
                lock_guard<mutex> guard(output_mutex);
                if (!candidate.empty()) {
                    if (!original_cases[i].finish(candidate)) throw runtime_error("Packed trajectory differs from original physics");
                    result[i] = move(candidate);
                    write_programs(options["outdir"] + "/" + filename, result);
                }
                int completed = ++done;
                if (completed % 10 == 0 || !result[i].empty()) {
                    double seconds = chrono::duration<double>(chrono::steady_clock::now() - began).count();
                    fprintf(stderr, "%s case=%d %zu->%zu completed=%d time=%.1fs\n", name.c_str(), i,
                        baseline[i].size(), result[i].empty() ? baseline[i].size() : result[i].size(), completed, seconds);
                }
            }
        };
        vector<thread> threads;
        for (int i = 0; i < workers; ++i) threads.emplace_back(worker);
        for (auto& thread : threads) thread.join();
        write_programs(options["outdir"] + "/" + filename, result);
        long before = 0, after = 0;
        for (size_t i = 0; i < cases.size(); ++i) { before += baseline[i].size(); after += result[i].empty() ? baseline[i].size() : result[i].size(); }
        printf("%s: %ld -> %ld\n", name.c_str(), before, after); fflush(stdout);
    }
}
