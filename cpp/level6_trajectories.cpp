#include "level6_packed.hpp"

// Large-neighbourhood search over complete emitter trajectories. Four emitter
// timelines are fixed; two are rebuilt from scratch after compressing time.
// Backward reachability masks safely prune matter that can no longer be fixed.
template<int W> class PairSearch {
    using Bits = SmallB<W>;
    struct Move { int state; char op; };
    struct Physics { Bits two, three, one; };
    struct FixedFuture { Bits zero{}, one{}, two{}, three{}; };
    struct Node {
        Bits matter{};
        int a = 0, b = 0, c = 0, captures = 0, parent = -1;
        char op_a = '-', op_b = '-', op_c = '-';
        double score = 0;
    };
    const Case& cs;
    int emit_a, emit_b, emit_c, count, width, length;
    double alpha, potential;
    Bits target, halo, relevant;
    vector<Bits> rays[3], single_future[3];
    vector<FixedFuture> fixed_future;
    vector<vector<Move>> moves;
    vector<Bits> fixed_one, fixed_two, fixed_three, fixed_any;
    vector<Physics> reachable;
    Program skeleton;
    Rng rng;
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
    Physics triple_future(int tick, int a, int b, int c) const {
        int remaining = length - tick;
        const Bits& x = single_future[0][remaining * count + a];
        const Bits& y = single_future[1][remaining * count + b];
        const Bits& z = single_future[2][remaining * count + c];
        Bits one = bor(bor(x, y), z), two = bor(bor(band(x, y), band(x, z)), band(y, z)), three = band(band(x, y), z);
        const auto& f = fixed_future[tick];
        return {bor(bor(band(f.zero, two), band(f.one, one)), f.two),
                bor(bor(bor(f.three, band(f.two, one)), band(f.one, two)), band(f.zero, three)), {}};
    }
    u64 hash(const Node& node) const {
        u64 h = node.a + 257ULL * node.b + 65537ULL * node.c + 16777259ULL * node.captures;
        for (u64 word : node.matter.w) { h ^= word + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); }
        return h;
    }
public:
    PairSearch(const Case& c, Program fixed, int a, int b, int beam_width,
               double halo_weight, double potential_weight, u64 seed, int third = -1)
        : cs(c), emit_a(a), emit_b(b), emit_c(third), count(2 * (2 * c.n + 1)), width(beam_width), length(fixed.size()),
          alpha(halo_weight), potential(potential_weight), target(smallBits<W>(c.P)), halo(smallBits<W>(c.H)),
          relevant(bor(target, halo)), moves(count), skeleton(move(fixed)), rng(seed) {
        for (int state = 0; state < count; ++state) {
            int offset = state / 2 - cs.n, active = state % 2;
            moves[state].push_back({state, '-'});
            if (offset < cs.n) moves[state].push_back({state + 2, 'F'});
            if (offset > -cs.n) moves[state].push_back({state - 2, 'B'});
            moves[state].push_back({state ^ 1, 'T'});
            for (int k = 0; k < (emit_c < 0 ? 2 : 3); ++k) {
                int emitter = k == 0 ? emit_a : k == 1 ? emit_b : emit_c;
                int raw = (cs.home(emitter) + offset + cs.L) % cs.L;
                rays[k].push_back(active ? smallBits<W>(cs.ray[emitter][raw]) : Bits{});
            }
        }
        int pos[6], active = 0;
        for (int e = 0; e < 6; ++e) pos[e] = cs.home(e);
        for (const auto& tick : skeleton) {
            Bits one{}, two{}, three{};
            for (int e = 0; e < 6; ++e) {
                if (e == emit_a || e == emit_b || e == emit_c) continue;
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
        if (emit_c >= 0) {
            fixed_future.resize(length + 1);
            for (int t = length - 1; t >= 0; --t) {
                const auto& later = fixed_future[t + 1];
                fixed_future[t] = {bor(later.zero, bandn(relevant, fixed_any[t])), bor(later.one, fixed_one[t]),
                                   bor(later.two, fixed_two[t]), bor(later.three, fixed_three[t])};
            }
            for (int k = 0; k < 3; ++k) {
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
                at[a * count + b] = {band(bor(now.two, later.two), target), band(bor(now.three, later.three), halo)};
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
        int home = 2 * cs.n;
        auto possible = emit_c < 0 ? future(0, home, home) : triple_future(0, home, home, home);
        if (!bnone(bandn(target, possible.two))) return {};
        Node initial; initial.a = initial.b = initial.c = home;
        const vector<Move> idle{{home, '-'}};
        vector<vector<Node>> layers{{initial}};
        for (int t = 0; t < length; ++t) {
            vector<Node> children;
            children.reserve(layers.back().size() * (emit_c < 0 ? 16 : 64));
            for (int parent = 0; parent < static_cast<int>(layers.back().size()); ++parent) {
                const auto& old = layers.back()[parent];
                for (auto a : moves[old.a]) for (auto b : moves[old.b]) {
                    auto pair_physics = config(t, a.state, b.state);
                    for (auto c : (emit_c < 0 ? idle : moves[old.c])) {
                    auto phys = pair_physics;
                    if (emit_c >= 0) {
                        const auto& ray = rays[2][c.state];
                        phys.two = bor(bandn(pair_physics.two, ray), band(pair_physics.one, ray));
                        phys.three = bor(pair_physics.three, band(pair_physics.two, ray));
                    }
                    Node next;
                    next.a = a.state; next.b = b.state; next.c = c.state; next.parent = parent;
                    next.op_a = a.op; next.op_b = b.op; next.op_c = c.op; next.captures = old.captures;
                    next.matter = band(bandn(bor(old.matter, phys.two), phys.three), relevant);
                    if (beq(band(next.matter, target), target) && bnone(band(next.matter, halo))) {
                        ++next.captures; next.matter = bandn(next.matter, target);
                        if (next.captures == cs.req) {
                            Program result(skeleton.begin(), skeleton.begin() + t + 1);
                            result[t][emit_a] = a.op; result[t][emit_b] = b.op;
                            if (emit_c >= 0) result[t][emit_c] = c.op;
                            int index = parent;
                            for (int j = t - 1; j >= 0; --j) {
                                const auto& node = layers[j + 1][index];
                                result[j][emit_a] = node.op_a; result[j][emit_b] = node.op_b;
                                if (emit_c >= 0) result[j][emit_c] = node.op_c;
                                index = node.parent;
                            }
                            if (!cs.finish(result)) throw runtime_error("Trajectory reconstruction failed simulation");
                            return result;
                        }
                    }
                    if (cs.req - next.captures > length - t - 1) continue;
                    auto remaining = emit_c < 0 ? future(t + 1, a.state, b.state) : triple_future(t + 1, a.state, b.state, c.state);
                    Bits missing = bandn(target, next.matter), dirty = band(halo, next.matter);
                    if (!bnone(bandn(missing, remaining.two)) || !bnone(bandn(dirty, remaining.three))) continue;
                    double pot = 0;
                    for (int k = 0; k < (emit_c < 0 ? 2 : 3); ++k) {
                        int state = k == 0 ? a.state : k == 1 ? b.state : c.state;
                        const auto& ray = rays[k][state | 1];
                        pot += pc(band(ray, missing)) + 2 * pc(band(ray, dirty));
                    }
                    next.score = 10000.0 * next.captures + 4.0 * pc(band(target, next.matter)) -
                                 4.0 * alpha * pc(dirty) + potential * pot + 0.8 * rng.uni();
                    children.push_back(next);
                    }
                }
            }
            if (children.empty()) return {};
            sort(children.begin(), children.end(), [](const Node& a, const Node& b) { return a.score > b.score; });
            vector<Node> next;
            unordered_set<u64> seen;
            for (auto& child : children) {
                if (!seen.insert(hash(child)).second) continue;
                next.push_back(move(child));
                if (static_cast<int>(next.size()) == width) break;
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
            int remove = idle.empty() ? min(cut, static_cast<int>(column.size()) - 1) : idle[rng.below(idle.size())];
            column.erase(column.begin() + remove);
        }
        for (int t = 0; t < n - drop; ++t) result[t][e] = column[t];
    }
    return result;
}
template<int W> static Program optimize(const Case& cs, Program best, int trials, int width, int max_drop, int freed, Rng& rng) {
    const size_t before = best.size();
    vector<pair<int, int>> pairs;
    for (int a = 0; a < 6; ++a) for (int b = a + 1; b < 6; ++b) pairs.push_back({a, b});
    for (int trial = 0; trial < trials && best.size() > static_cast<size_t>(cs.req); ++trial) {
        auto [a, b] = pairs[(trial + rng.below(pairs.size())) % pairs.size()];
        int drop = 1 + (trial % 4 == 3 ? rng.below(min(max_drop, static_cast<int>(best.size()) - cs.req)) : 0);
        Program draft = compress(best, drop, rng);
        if (cs.finish(draft)) { best = move(draft); continue; }
        double alpha = trial % 3 == 0 ? 0.5 : trial % 3 == 1 ? 2 : 6;
        int c = -1;
        if (freed == 3) { do { c = rng.below(6); } while (c == a || c == b); }
        PairSearch<W> search(cs, move(draft), a, b, width, alpha, 0.4, rng.next(), c);
        auto candidate = search.run();
        if (!candidate.empty() && candidate.size() < best.size()) best = move(candidate);
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
        {"trials", "40"}, {"drop", "3"}, {"freed", "2"}, {"seed", "1"}, {"cases", ""}};
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i]; auto equal = arg.find('=');
        if (equal == string::npos || !options.count(arg.substr(0, equal))) { cerr << "Unknown option: " << arg << '\n'; return 1; }
        options[arg.substr(0, equal)] = arg.substr(equal + 1);
    }
    int width = stoi(options["width"]), trials = stoi(options["trials"]), workers = stoi(options["threads"]), drop = stoi(options["drop"]);
    if (min({width, trials, workers, drop}) < 1) return 1;
    int freed = stoi(options["freed"]);
    if (freed != 2 && freed != 3) return 1;
    int low = 0, high = INT_MAX;
    if (!options["cases"].empty()) { auto limits = split(options["cases"], '-'); low = stoi(limits[0]); high = limits.size() > 1 ? stoi(limits[1]) : low; }
    for (const auto& name : split(options["names"])) {
        auto original_cases = load_cases("level-6/in_level-6_" + name + ".txt");
        vector<Case> cases;
        for (const auto& cs : original_cases) cases.push_back(packed_relevant(cs));
        string filename = "out_level-6_" + name + ".txt";
        auto baseline = read_programs(options["baseline"] + "/" + filename, cases.size());
        for (size_t i = 0; i < cases.size(); ++i) if (!cases[i].finish(baseline[i])) return 2;
        vector<Program> result(cases.size()); atomic<int> next(0), done(0); mutex output_mutex;
        auto began = chrono::steady_clock::now();
        auto worker = [&]() {
            for (;;) {
                int i = next++; if (i >= static_cast<int>(cases.size())) break;
                if (i < low || i > high) continue;
                const auto& cs = cases[i]; Rng rng(stoull(options["seed"]) * 1009 + i * 31);
                Program candidate;
                switch ((pc(bor(cs.P, cs.H)) + 63) / 64) {
                    case 1: candidate = optimize<1>(cs, baseline[i], trials, width, drop, freed, rng); break;
                    case 2: candidate = optimize<2>(cs, baseline[i], trials, width, drop, freed, rng); break;
                    case 3: candidate = optimize<3>(cs, baseline[i], trials, width, drop, freed, rng); break;
                    case 4: candidate = optimize<4>(cs, baseline[i], trials, width, drop, freed, rng); break;
                    case 5: candidate = optimize<5>(cs, baseline[i], trials, width, drop, freed, rng); break;
                    case 6: candidate = optimize<6>(cs, baseline[i], trials, width, drop, freed, rng); break;
                    default: candidate = optimize<7>(cs, baseline[i], trials, width, drop, freed, rng);
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
