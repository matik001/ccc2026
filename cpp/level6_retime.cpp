#include "level6_packed.hpp"

// Reschedule all six command streams together. Each non-idle command keeps
// its order within an emitter's path, while every inter-emitter timing may
// change. Optional cancelling move pairs change the paths themselves.
template<int W> static Program retime(const Case& cs, const Program& source,
        int horizon, int width, int variant, Rng& rng) {
    using Bits = SmallB<W>;
    struct Path { string ops; vector<Bits> ray, future; vector<int> pos, active; } paths[6];
    const Bits target = smallBits<W>(cs.P), halo = smallBits<W>(cs.H), relevant = bor(target, halo);
    int sizes[6];
    for (int e = 0; e < 6; ++e) {
        auto& path = paths[e];
        for (const auto& tick : source) if (tick[e] != '-') path.ops += tick[e];
        if (variant % 3 != 0 || static_cast<int>(path.ops.size()) > horizon) {
            vector<pair<int, int>> cancellations;
            for (int a = 0; a < static_cast<int>(path.ops.size()); ++a)
                for (int b = a + 1; b < min(a + 5, static_cast<int>(path.ops.size())); ++b)
                    if ((path.ops[a] == 'F' && path.ops[b] == 'B') || (path.ops[a] == 'B' && path.ops[b] == 'F') ||
                        (path.ops[a] == 'T' && path.ops[b] == 'T')) cancellations.push_back({a, b});
            if (!cancellations.empty() && (static_cast<int>(path.ops.size()) > horizon || rng.uni() < 0.5)) {
                auto [a, b] = cancellations[rng.below(cancellations.size())];
                path.ops.erase(path.ops.begin() + b); path.ops.erase(path.ops.begin() + a);
            }
        }
        if (static_cast<int>(path.ops.size()) > horizon) path.ops.resize(horizon);
        sizes[e] = path.ops.size();
        int pos = cs.home(e), active = 0;
        path.ray.push_back({}); path.pos.push_back(pos); path.active.push_back(active);
        for (char op : path.ops) {
            if (op == 'F') pos = (pos + 1) % cs.L;
            else if (op == 'B') pos = (pos + cs.L - 1) % cs.L;
            else if (op == 'T') active ^= 1;
            path.pos.push_back(pos); path.active.push_back(active);
            path.ray.push_back(active ? smallBits<W>(cs.ray[e][pos]) : Bits{});
        }
        path.future.resize(path.ray.size());
        Bits future{};
        for (int i = sizes[e]; i >= 0; --i) { future = bor(future, path.ray[i]); path.future[i] = future; }
    }
    struct Node { array<int, 6> at{}; Bits matter{}; int captures = 0, parent = -1, choice = 0; double score = 0; };
    struct Pair { int bits; Bits one, two; Bits future_one, future_two; double progress; };
    vector<vector<Node>> layers(1, vector<Node>(1));
    double alpha = variant % 4 == 0 ? 0.3 : variant % 4 == 1 ? 1.0 : variant % 4 == 2 ? 3.0 : 8.0;
    for (int t = 0; t < horizon; ++t) {
        vector<Node> candidates;
        candidates.reserve(layers.back().size() * 64);
        for (int parent = 0; parent < static_cast<int>(layers.back().size()); ++parent) {
            const auto& old = layers.back()[parent];
            vector<Pair> pairs[3];
            for (int axis = 0; axis < 3; ++axis) {
                int a = axis, b = axis + 3;
                for (int da = 0; da <= 1; ++da) for (int db = 0; db <= 1; ++db) {
                    int ia = old.at[a] + da, ib = old.at[b] + db;
                    if (ia > sizes[a] || ib > sizes[b]) continue;
                    if (sizes[a] - ia > horizon - t - 1 || sizes[b] - ib > horizon - t - 1) continue;
                    const auto& ra = paths[a].ray[ia]; const auto& rb = paths[b].ray[ib];
                    const auto& fa = paths[a].future[ia]; const auto& fb = paths[b].future[ib];
                    double progress = static_cast<double>(ia) / max(1, sizes[a]) + static_cast<double>(ib) / max(1, sizes[b]);
                    pairs[axis].push_back({(da << a) | (db << b), bxor(ra, rb), band(ra, rb), bxor(fa, fb), band(fa, fb), progress});
                }
            }
            for (const auto& a : pairs[0]) for (const auto& b : pairs[1]) {
                Bits any_a = bor(a.one, a.two), any_b = bor(b.one, b.two);
                Bits one = bandn(bxor(a.one, b.one), bor(a.two, b.two));
                Bits two = bor(bor(bandn(a.two, any_b), bandn(b.two, any_a)), band(a.one, b.one));
                Bits three = bor(band(a.two, any_b), band(b.two, any_a));
                Bits any = bor(bor(one, two), three);
                Bits f_any_a = bor(a.future_one, a.future_two), f_any_b = bor(b.future_one, b.future_two);
                Bits f_one = bandn(bxor(a.future_one, b.future_one), bor(a.future_two, b.future_two));
                Bits f_two = bor(bor(bandn(a.future_two, f_any_b), bandn(b.future_two, f_any_a)), band(a.future_one, b.future_one));
                Bits f_three = bor(band(a.future_two, f_any_b), band(b.future_two, f_any_a));
                Bits f_any = bor(bor(f_one, f_two), f_three);
                for (const auto& c : pairs[2]) {
                    Bits c_any = bor(c.one, c.two);
                    Bits exact_two = bor(bor(bandn(two, c_any), band(one, c.one)), bandn(c.two, any));
                    Bits at_least_three = bor(bor(three, band(two, c_any)), band(one, c.two));
                    Node next;
                    next.parent = parent; next.choice = a.bits | b.bits | c.bits; next.captures = old.captures;
                    for (int e = 0; e < 6; ++e) next.at[e] = old.at[e] + ((next.choice >> e) & 1);
                    next.matter = band(bandn(bor(old.matter, exact_two), at_least_three), relevant);
                    if (beq(band(next.matter, target), target) && bnone(band(next.matter, halo))) {
                        ++next.captures; next.matter = bandn(next.matter, target);
                        if (next.captures == cs.req) {
                            vector<int> choices{next.choice};
                            int index = parent;
                            for (int j = t; j > 0; --j) { const auto& node = layers[j][index]; choices.push_back(node.choice); index = node.parent; }
                            reverse(choices.begin(), choices.end());
                            Program result; int at[6]{};
                            for (int choice : choices) {
                                string tick(6, '-');
                                for (int e = 0; e < 6; ++e) if (choice & (1 << e)) tick[e] = paths[e].ops[at[e]++];
                                result.push_back(tick);
                            }
                            if (!cs.finish(result)) throw runtime_error("Retiming reconstruction failed simulation");
                            return result;
                        }
                    }
                    if (cs.req - next.captures > horizon - t - 1) continue;
                    Bits missing = bandn(target, next.matter), dirty = band(halo, next.matter);
                    Bits fc_any = bor(c.future_one, c.future_two);
                    Bits possible_three = bor(bor(f_three, band(f_two, fc_any)), band(f_one, c.future_two));
                    Bits possible_two = bor(bor(bor(bandn(f_two, fc_any), band(f_one, c.future_one)), bandn(c.future_two, f_any)), possible_three);
                    if (!bnone(bandn(missing, possible_two)) || !bnone(bandn(dirty, possible_three))) continue;
                    double capture_value = variant % 2 ? 4.0 * cs.pcount + 4 : 10000.0;
                    next.score = capture_value * next.captures + 4.0 * pc(band(next.matter, target)) -
                                 4.0 * alpha * pc(dirty) + 0.15 * (a.progress + b.progress + c.progress) + rng.uni();
                    candidates.push_back(next);
                }
            }
        }
        if (candidates.empty()) return {};
        sort(candidates.begin(), candidates.end(), [](const Node& a, const Node& b) { return a.score > b.score; });
        vector<Node> next; unordered_set<u64> seen;
        for (auto& node : candidates) {
            u64 hash = node.captures + 1469598103934665603ULL;
            for (int index : node.at) { hash ^= index; hash *= 1099511628211ULL; }
            for (auto word : node.matter.w) { hash ^= word; hash *= 1099511628211ULL; }
            if (!seen.insert(hash).second) continue;
            next.push_back(move(node)); if (static_cast<int>(next.size()) == width) break;
        }
        layers.push_back(move(next));
    }
    return {};
}
template<int W> static Program optimize(const Case& cs, Program best, int trials, int width, Rng& rng) {
    size_t before = best.size();
    for (int trial = 0; trial < trials && best.size() > static_cast<size_t>(cs.req); ++trial) {
        auto candidate = retime<W>(cs, best, best.size() - 1, width, trial, rng);
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
        {"outdir", "cpp/candidates/retime"}, {"threads", "3"}, {"width", "500"},
        {"trials", "8"}, {"seed", "1"}, {"cases", ""}};
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i]; auto equal = arg.find('=');
        if (equal == string::npos || !options.count(arg.substr(0, equal))) { cerr << "Unknown option: " << arg << '\n'; return 1; }
        options[arg.substr(0, equal)] = arg.substr(equal + 1);
    }
    int width = stoi(options["width"]), trials = stoi(options["trials"]), workers = stoi(options["threads"]);
    if (min({width, trials, workers}) < 1) return 1;
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
                    case 1: candidate = optimize<1>(cs, baseline[i], trials, width, rng); break;
                    case 2: candidate = optimize<2>(cs, baseline[i], trials, width, rng); break;
                    case 3: candidate = optimize<3>(cs, baseline[i], trials, width, rng); break;
                    case 4: candidate = optimize<4>(cs, baseline[i], trials, width, rng); break;
                    case 5: candidate = optimize<5>(cs, baseline[i], trials, width, rng); break;
                    case 6: candidate = optimize<6>(cs, baseline[i], trials, width, rng); break;
                    default: candidate = optimize<7>(cs, baseline[i], trials, width, rng);
                }
                lock_guard<mutex> guard(output_mutex);
                if (!candidate.empty()) {
                    if (!original_cases[i].finish(candidate)) throw runtime_error("Packed retiming differs from original physics");
                    result[i] = move(candidate); write_programs(options["outdir"] + "/" + filename, result);
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
