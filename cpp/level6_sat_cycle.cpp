#include "level6_core.hpp"
#include <z3++.h>

// Exact bounded search for a periodic schedule. The two emitters on an axis
// may exchange roles at the end of the period.
static Program cycle(const Case& cs, int period, int captures, int limit,
                     int timeout, unsigned seed, int& status) {
    z3::context ctx;
    z3::solver solver(ctx, "QF_BV");
    solver.set("timeout", static_cast<unsigned>(timeout));
    solver.set("random_seed", seed);
    unsigned bits = 1;
    while ((1u << bits) <= static_cast<unsigned>(2 * cs.n)) ++bits;
    vector<z3::expr_vector> pos, active;
    for (int t = 0; t <= period; ++t) {
        pos.emplace_back(ctx); active.emplace_back(ctx);
        for (int e = 0; e < 6; ++e) {
            string name = to_string(t) + "_" + to_string(e);
            auto p = ctx.bv_const(("p_" + name).c_str(), bits);
            auto a = ctx.bool_const(("a_" + name).c_str());
            solver.add(z3::ule(p, ctx.bv_val(2 * cs.n, bits)));
            if (t) {
                const auto& old = pos[t - 1][e];
                solver.add(p == old || p == old + 1 || p == old - 1);
                solver.add(z3::implies(a != active[t - 1][e], p == old));
            }
            pos[t].push_back(p); active[t].push_back(a);
        }
    }
    z3::expr_vector swaps(ctx);
    for (int axis = 0; axis < 3; ++axis) {
        solver.add(z3::ule(pos[0][axis], pos[0][axis + 3]));
        solver.add(z3::implies(pos[0][axis] == pos[0][axis + 3],
                              z3::implies(active[0][axis], active[0][axis + 3])));
        auto exchange = ctx.bool_const(("swap_" + to_string(axis)).c_str());
        swaps.push_back(exchange);
        for (int e : {axis, axis + 3}) {
            int other = (e + 3) % 6;
            solver.add(pos[period][e] == z3::ite(exchange, pos[0][other], pos[0][e]));
            solver.add(active[period][e] == z3::ite(exchange, active[0][other], active[0][e]));
        }
    }
    vector<int> cells;
    auto relevant = bor(cs.P, cs.H);
    for (int i = 0; i < 3 * cs.n * (cs.n - 1) + 1; ++i)
        if ((relevant.w[i / 64] >> (i % 64)) & 1) cells.push_back(i);
    vector<array<int, 6>> coordinates(cells.size());
    vector<bool> target;
    for (size_t i = 0; i < cells.size(); ++i) {
        target.push_back((cs.P.w[cells[i] / 64] >> (cells[i] % 64)) & 1);
        for (int e = 0; e < 6; ++e) {
            coordinates[i][e] = -1;
            for (int offset = 1 - cs.n; offset < cs.n; ++offset) {
                int raw = (cs.home(e) + (e < 3 ? offset : -offset) + cs.L) % cs.L;
                if ((cs.ray[e][raw].w[cells[i] / 64] >> (cells[i] % 64)) & 1)
                    coordinates[i][e] = offset + cs.n;
            }
            if (coordinates[i][e] < 0) throw runtime_error("Missing ray coordinate");
        }
    }
    const int weights[6] = {1, 1, 1, 1, 1, 1};
    z3::expr_vector matter(ctx), collected(ctx);
    for (int t = 0; t <= period; ++t) {
        z3::expr_vector next(ctx);
        z3::expr complete = ctx.bool_val(true);
        for (size_t i = 0; i < cells.size(); ++i) {
            z3::expr_vector hits(ctx);
            for (int e = 0; e < 6; ++e)
                hits.push_back(active[t][e] && pos[t][e] == ctx.bv_val(coordinates[i][e], bits));
            auto two = z3::pbeq(hits, weights, 2);
            auto three = z3::pbge(hits, weights, 3);
            auto value = ctx.bool_const(("m_" + to_string(t) + "_" + to_string(i)).c_str());
            // Worst-case target is empty at the start; the setup can only add
            // target cells. Halo is exactly what the final setup tick creates.
            if (!t) solver.add(value == (target[i] ? ctx.bool_val(false) : two));
            else solver.add(value == ((matter[i] || two) && !three));
            next.push_back(value);
            complete = complete && (target[i] ? value : !value);
        }
        if (t) {
            auto capture = ctx.bool_const(("c_" + to_string(t)).c_str());
            solver.add(capture == complete);
            collected.push_back(capture);
            for (size_t i = 0; i < cells.size(); ++i) if (target[i]) {
                auto cleared = next[i] && !capture;
                next.set(i, cleared);
            }
        }
        matter = next;
    }
    vector<int> capture_weights(period, 1);
    solver.add(z3::pbge(collected, capture_weights.data(), captures));
    solver.add(collected[period - 1]);
    auto answer = solver.check();
    status = answer == z3::sat ? 1 : answer == z3::unsat ? 0 : -1;
    if (answer != z3::sat) return {};
    auto model = solver.get_model();
    int exchange = 0;
    for (int axis = 0; axis < 3; ++axis)
        if (model.eval(swaps[axis], true).is_true()) exchange |= 1 << axis;
    Program loop;
    for (int t = 1; t <= period; ++t) {
        string tick(6, '-');
        for (int e = 0; e < 6; ++e) {
            int before = model.eval(pos[t - 1][e], true).get_numeral_uint();
            int after = model.eval(pos[t][e], true).get_numeral_uint();
            if (model.eval(active[t - 1][e], true).is_true() != model.eval(active[t][e], true).is_true()) tick[e] = 'T';
            else if (before != after) tick[e] = ((after > before) == (e < 3)) ? 'F' : 'B';
        }
        loop.push_back(tick);
    }
    if (exchange) for (int t = 0; t < period; ++t) {
        string tick = loop[t];
        for (int axis = 0; axis < 3; ++axis) if (exchange & (1 << axis)) {
            auto invert = [](char c) { return c == 'F' ? 'B' : c == 'B' ? 'F' : c; };
            char a = tick[axis], b = tick[axis + 3];
            tick[axis] = invert(b); tick[axis + 3] = invert(a);
        }
        loop.push_back(tick);
    }
    Program best;
    for (int phase = 0; phase < static_cast<int>(loop.size()); ++phase) {
        int setup = 0, offsets[6], enabled[6];
        for (int e = 0; e < 6; ++e) {
            int source = e;
            if (phase >= period && (exchange & (1 << (e % 3)))) source = (e + 3) % 6;
            offsets[e] = static_cast<int>(model.eval(pos[phase % period][source], true).get_numeral_uint()) - cs.n;
            if (e >= 3) offsets[e] = -offsets[e];
            enabled[e] = model.eval(active[phase % period][source], true).is_true();
            setup = max(setup, abs(offsets[e]) + enabled[e]);
        }
        Program result;
        for (int t = 1; t <= setup; ++t) {
            string tick(6, '-');
            for (int e = 0; e < 6; ++e) {
                int first = setup - abs(offsets[e]) - enabled[e] + 1;
                if (first <= t && t < first + abs(offsets[e])) tick[e] = offsets[e] > 0 ? 'F' : 'B';
                else if (enabled[e] && t == setup) tick[e] = 'T';
            }
            result.push_back(tick);
        }
        for (int t = 0; static_cast<int>(result.size()) <= limit; ++t)
            result.push_back(loop[(phase + t) % loop.size()]);
        if (cs.finish(result) && static_cast<int>(result.size()) <= limit) {
            limit = result.size() - 1;
            best = move(result);
        }
    }
    return best;
}

static vector<string> split(const string& input, char sep) {
    vector<string> result; stringstream stream(input); string value;
    while (getline(stream, value, sep)) if (!value.empty()) result.push_back(value);
    return result;
}

int main(int argc, char** argv) {
    map<string, string> options{{"names", "2-medium,3-large"}, {"baseline", "cpp/outputs"},
        {"outdir", "cpp/candidates/sat_cycles"}, {"specs", "2:1,3:1,4:1,5:2,5:1,7:2,6:1"},
        {"timeout", "3000"}, {"threads", "3"}, {"seed", "1"}, {"minreq", "3"}, {"minlen", "30"}, {"cases", ""}};
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i]; auto equal = arg.find('=');
        if (equal == string::npos || !options.count(arg.substr(0, equal))) { cerr << "Unknown option: " << arg << '\n'; return 1; }
        options[arg.substr(0, equal)] = arg.substr(equal + 1);
    }
    int workers = stoi(options["threads"]), timeout = stoi(options["timeout"]), seed = stoi(options["seed"]);
    if (workers < 1 || timeout < 1) return 1;
    vector<pair<int, int>> specs;
    for (const auto& spec : split(options["specs"], ',')) {
        auto parts = split(spec, ':');
        if (parts.size() != 2 || stoi(parts[0]) < 1 || stoi(parts[1]) < 1 || stoi(parts[1]) > stoi(parts[0])) return 1;
        specs.emplace_back(stoi(parts[0]), stoi(parts[1]));
    }
    int low = 0, high = INT_MAX;
    if (!options["cases"].empty()) { auto range = split(options["cases"], '-'); low = stoi(range[0]); high = range.size() > 1 ? stoi(range[1]) : low; }
    for (const auto& name : split(options["names"], ',')) {
        auto cases = load_cases("level-6/in_level-6_" + name + ".txt");
        string filename = "out_level-6_" + name + ".txt";
        auto baseline = read_programs(options["baseline"] + "/" + filename, cases.size());
        for (size_t i = 0; i < cases.size(); ++i) if (!cases[i].finish(baseline[i])) return 2;
        vector<Program> result(cases.size()); atomic<int> next(0), done(0), sat(0), unsat(0), unknown(0); mutex lock;
        auto began = chrono::steady_clock::now();
        auto worker = [&]() {
            for (;;) {
                int i = next++; if (i >= static_cast<int>(cases.size())) break;
                const auto& cs = cases[i];
                if (i < low || i > high || cs.req < stoi(options["minreq"]) || static_cast<int>(baseline[i].size()) < stoi(options["minlen"])) continue;
                Program best = baseline[i];
                for (auto [period, captures] : specs) {
                    if (period * (cs.req / captures) >= static_cast<int>(best.size())) continue;
                    int status;
                    auto candidate = cycle(cs, period, captures, best.size() - 1, timeout, seed * 1009 + i * 31 + period, status);
                    if (status == 1) ++sat; else if (status == 0) ++unsat; else ++unknown;
                    if (!candidate.empty()) best = move(candidate);
                }
                lock_guard<mutex> guard(lock);
                if (best.size() < baseline[i].size()) { result[i] = best; write_programs(options["outdir"] + "/" + filename, result); }
                int count = ++done;
                if (count % 5 == 0 || best.size() < baseline[i].size()) {
                    double seconds = chrono::duration<double>(chrono::steady_clock::now() - began).count();
                    fprintf(stderr, "%s case=%d %zu->%zu done=%d SAT/UNSAT/timeout=%d/%d/%d time=%.1fs\n", name.c_str(), i,
                        baseline[i].size(), best.size(), count, sat.load(), unsat.load(), unknown.load(), seconds);
                }
            }
        };
        vector<thread> threads;
        for (int i = 0; i < workers; ++i) threads.emplace_back(worker);
        for (auto& t : threads) t.join();
        write_programs(options["outdir"] + "/" + filename, result);
        long before = 0, after = 0;
        for (size_t i = 0; i < cases.size(); ++i) { before += baseline[i].size(); after += result[i].empty() ? baseline[i].size() : result[i].size(); }
        printf("%s: %ld -> %ld\n", name.c_str(), before, after); fflush(stdout);
    }
}
