#include "level6_core.hpp"
#include <z3++.h>

struct Frame {
    array<int, 6> pos{};
    int active = 0, captures = 0;
    B matter = bzero();
};

static vector<Frame> simulate(const Case& cs, const Program& program) {
    vector<Frame> frames(program.size() + 1);
    for (int e = 0; e < 6; ++e) frames[0].pos[e] = cs.home(e);
    for (size_t t = 0; t < program.size(); ++t) {
        Frame f = frames[t];
        for (int e = 0; e < 6; ++e) {
            char op = program[t][e];
            if (op == 'F') f.pos[e] = (f.pos[e] + 1) % cs.L;
            else if (op == 'B') f.pos[e] = (f.pos[e] + cs.L - 1) % cs.L;
            else if (op == 'T') f.active ^= 1 << e;
        }
        B two, three;
        cs.config(f.pos.data(), f.active, two, three);
        f.matter = bandn(bor(f.matter, two), three);
        if (beq(band(f.matter, cs.P), cs.P) && bnone(band(f.matter, cs.H))) {
            ++f.captures; f.matter = bandn(f.matter, cs.P);
        }
        frames[t + 1] = f;
    }
    return frames;
}

static Program solve_suffix(const Case& cs, const Frame& start, int horizon, int timeout, unsigned seed,
                            const Frame* end = nullptr) {
    if (horizon < 1) return {};
    z3::context ctx;
    z3::solver solver(ctx);
    solver.set("timeout", static_cast<unsigned>(timeout));
    solver.set("random_seed", seed);
    unsigned bits = 1;
    while ((1u << bits) <= static_cast<unsigned>(2 * cs.n)) ++bits;
    vector<z3::expr_vector> positions, enabled;
    positions.emplace_back(ctx); enabled.emplace_back(ctx);
    for (int e = 0; e < 6; ++e) {
        int offset = cs.delta(cs.home(e), start.pos[e]) * (e < 3 ? 1 : -1);
        if (abs(offset) > cs.n) return {};
        positions[0].push_back(ctx.bv_val(offset + cs.n, bits));
        enabled[0].push_back(ctx.bool_val((start.active >> e) & 1));
    }
    vector<int> cells;
    B relevant = bor(cs.P, cs.H);
    for (int cell = 0; cell < 3 * cs.n * (cs.n - 1) + 1; ++cell)
        if ((relevant.w[cell / 64] >> (cell % 64)) & 1) cells.push_back(cell);
    vector<array<int, 6>> coordinates(cells.size());
    for (size_t i = 0; i < cells.size(); ++i) for (int e = 0; e < 6; ++e) {
        coordinates[i][e] = -1;
        for (int offset = -cs.n + 1; offset < cs.n; ++offset) {
            int p = (cs.home(e) + (e < 3 ? offset : -offset) + cs.L) % cs.L;
            if ((cs.ray[e][p].w[cells[i] / 64] >> (cells[i] % 64)) & 1) coordinates[i][e] = offset + cs.n;
        }
        if (coordinates[i][e] < 0) throw runtime_error("SAT: missing ray coordinate");
    }
    z3::expr_vector matter(ctx);
    for (int cell : cells) matter.push_back(ctx.bool_val((start.matter.w[cell / 64] >> (cell % 64)) & 1));
    for (int t = 1; t <= horizon; ++t) {
        positions.emplace_back(ctx); enabled.emplace_back(ctx);
        for (int e = 0; e < 6; ++e) {
            string name = to_string(t) + "_" + to_string(e);
            auto p = ctx.bv_const(("p_" + name).c_str(), bits);
            auto active = ctx.bool_const(("a_" + name).c_str());
            const auto& before = positions[t - 1][e];
            solver.add(z3::ule(p, ctx.bv_val(2 * cs.n, bits)));
            solver.add((p == before) || (p == before + 1) || (p == before - 1));
            solver.add(z3::implies(active != enabled[t - 1][e], p == before));
            positions[t].push_back(p); enabled[t].push_back(active);
        }
        z3::expr_vector next(ctx);
        for (size_t i = 0; i < cells.size(); ++i) {
            z3::expr_vector hits(ctx);
            for (int e = 0; e < 6; ++e)
                hits.push_back(enabled[t][e] && (positions[t][e] == ctx.bv_val(coordinates[i][e], bits)));
            const int weights[6] = {1, 1, 1, 1, 1, 1};
            auto create = z3::pbeq(hits, weights, 2);
            auto destroy = z3::pbge(hits, weights, 3);
            string name = "m_" + to_string(t) + "_" + to_string(i);
            auto value = ctx.bool_const(name.c_str());
            solver.add(value == ((matter[i] || create) && !destroy));
            next.push_back(value);
        }
        matter = next;
    }
    for (size_t i = 0; i < cells.size(); ++i) {
        bool target = (cs.P.w[cells[i] / 64] >> (cells[i] % 64)) & 1;
        solver.add(target ? matter[i] : !matter[i]);
    }
    if (end) for (int e = 0; e < 6; ++e) {
        int offset = cs.delta(cs.home(e), end->pos[e]) * (e < 3 ? 1 : -1);
        if (abs(offset) > cs.n) return {};
        solver.add(positions[horizon][e] == ctx.bv_val(offset + cs.n, bits));
        solver.add(enabled[horizon][e] == ctx.bool_val((end->active >> e) & 1));
    }
    if (solver.check() != z3::sat) return {};
    auto model = solver.get_model();
    Program result;
    for (int t = 1; t <= horizon; ++t) {
        string tick(6, '-');
        for (int e = 0; e < 6; ++e) {
            int a = model.eval(positions[t - 1][e], true).get_numeral_uint();
            int b = model.eval(positions[t][e], true).get_numeral_uint();
            bool active_before = model.eval(enabled[t - 1][e], true).is_true();
            bool active_after = model.eval(enabled[t][e], true).is_true();
            if (active_before != active_after) tick[e] = 'T';
            else if (b != a) tick[e] = ((b > a) == (e < 3)) ? 'F' : 'B';
        }
        result.push_back(tick);
    }
    return result;
}

static vector<string> split_sat(const string& input, char sep) {
    vector<string> result; stringstream stream(input); string token;
    while (getline(stream, token, sep)) if (!token.empty()) result.push_back(token);
    return result;
}

int main(int argc, char** argv) {
    map<string, string> options{{"names", "2-medium,3-large,4-enormous"}, {"baseline", "cpp/outputs"},
        {"outdir", "cpp/candidates/sat_suffix"}, {"windows", "6,10,16"}, {"timeout", "500"},
        {"threads", "3"}, {"seed", "1"}, {"cases", ""}, {"mode", "suffix"}};
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i]; size_t equal = arg.find('=');
        if (equal == string::npos || !options.count(arg.substr(0, equal))) { cerr << "Unknown option: " << arg << '\n'; return 1; }
        options[arg.substr(0, equal)] = arg.substr(equal + 1);
    }
    int timeout = stoi(options["timeout"]), workers = stoi(options["threads"]), seed = stoi(options["seed"]);
    if (timeout < 1 || workers < 1) return 1;
    if (options["mode"] != "suffix" && options["mode"] != "segments") return 1;
    vector<int> windows;
    for (auto& value : split_sat(options["windows"], ',')) { int window = stoi(value); if (window < 2) return 1; windows.push_back(window); }
    int low = 0, high = INT_MAX;
    if (!options["cases"].empty()) { auto limits = split_sat(options["cases"], '-'); low = stoi(limits[0]); high = limits.size() > 1 ? stoi(limits[1]) : low; }
    for (const auto& name : split_sat(options["names"], ',')) {
        auto cases = load_cases("level-6/in_level-6_" + name + ".txt");
        string filename = "out_level-6_" + name + ".txt";
        auto baseline = read_programs(options["baseline"] + "/" + filename, cases.size());
        for (size_t i = 0; i < cases.size(); ++i) if (!cases[i].finish(baseline[i])) return 2;
        vector<Program> result(cases.size()); atomic<int> next(0), completed(0); mutex output_mutex;
        auto began = chrono::steady_clock::now();
        auto worker = [&]() {
            for (;;) {
                int i = next++; if (i >= static_cast<int>(cases.size())) break;
                if (i < low || i > high) continue;
                const auto& cs = cases[i]; Program best = baseline[i];
                if (options["mode"] == "segments") {
                    map<string, Program> cache;
                    bool improved;
                    do {
                        improved = false;
                        auto frames = simulate(cs, best);
                        int begin = 0;
                        for (int end = 1; end <= static_cast<int>(best.size()) && !improved; ++end) {
                            if (frames[end].captures == frames[begin].captures) continue;
                            for (int window : windows) {
                                int cut = max(begin, end - window), horizon = end - cut - 1;
                                if (horizon < 1) continue;
                                const auto& first = frames[cut]; const auto& last = frames[end];
                                string key = to_string(horizon) + ":" + to_string(first.active) + ":" + to_string(last.active);
                                for (int e = 0; e < 6; ++e) key += ":" + to_string(first.pos[e]) + ":" + to_string(last.pos[e]);
                                auto relevant = band(first.matter, bor(cs.P, cs.H));
                                for (auto word : relevant.w) key += ":" + to_string(word);
                                auto found = cache.find(key);
                                if (found == cache.end()) {
                                    auto path = solve_suffix(cs, first, horizon, timeout, seed * 1009 + i * 31 + window, &last);
                                    found = cache.emplace(key, move(path)).first;
                                }
                                if (found->second.empty()) continue;
                                Program candidate(best.begin(), best.begin() + cut);
                                candidate.insert(candidate.end(), found->second.begin(), found->second.end());
                                candidate.insert(candidate.end(), best.begin() + end, best.end());
                                if (cs.finish(candidate) && candidate.size() < best.size()) {
                                    best = move(candidate); improved = true; break;
                                }
                            }
                            begin = end;
                        }
                    } while (improved);
                }
                int last_horizon = -1;
                for (int window : windows) {
                    auto frames = simulate(cs, best);
                    int capture_start = 0;
                    for (int t = 0; t < static_cast<int>(best.size()); ++t)
                        if (frames[t].captures == cs.req - 1) { capture_start = t; break; }
                    int cut = max(capture_start, static_cast<int>(best.size()) - window);
                    int horizon = best.size() - cut - 1;
                    if (horizon == last_horizon || horizon < 1) continue;
                    last_horizon = horizon;
                    Program suffix = solve_suffix(cs, frames[cut], horizon, timeout, seed * 1009 + i * 31 + window);
                    if (suffix.empty()) continue;
                    Program candidate(best.begin(), best.begin() + cut);
                    candidate.insert(candidate.end(), suffix.begin(), suffix.end());
                    if (!cs.finish(candidate)) throw runtime_error("SAT result failed independent simulation");
                    if (candidate.size() < best.size()) best = move(candidate);
                }
                lock_guard<mutex> guard(output_mutex);
                if (best.size() < baseline[i].size()) {
                    result[i] = move(best);
                    write_programs(options["outdir"] + "/" + filename, result);
                }
                int done = ++completed;
                if (done % 20 == 0 || !result[i].empty()) {
                    double seconds = chrono::duration<double>(chrono::steady_clock::now() - began).count();
                    fprintf(stderr, "%s case=%d %zu->%zu completed=%d time=%.1fs\n", name.c_str(), i,
                        baseline[i].size(), result[i].empty() ? baseline[i].size() : result[i].size(), done, seconds);
                }
            }
        };
        vector<thread> threads;
        for (int i = 0; i < workers; ++i) threads.emplace_back(worker);
        for (auto& thread : threads) thread.join();
        long before = 0, after = 0;
        for (size_t i = 0; i < baseline.size(); ++i) { before += baseline[i].size(); after += result[i].empty() ? baseline[i].size() : result[i].size(); }
        printf("%s: %ld -> %ld\n", name.c_str(), before, after); fflush(stdout);
        write_programs(options["outdir"] + "/" + filename, result);
    }
}
