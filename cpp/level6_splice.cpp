#include "level6_core.hpp"

// C++ version of the colleague's segment-recombination idea. Every capture
// empties P and H; outside matter never affects the relevant cells.
struct State {
    array<int, 6> pos{};
    int active = 0;
};
struct Canonical {
    array<int, 12> key{};
    int swaps = 0;
};
static Canonical canonical(const Case& cs, const State& state) {
    Canonical result;
    for (int axis = 0; axis < 3; ++axis) {
        pair<int, int> a{cs.delta(cs.home(axis), state.pos[axis]), (state.active >> axis) & 1};
        pair<int, int> b{-cs.delta(cs.home(axis + 3), state.pos[axis + 3]), (state.active >> (axis + 3)) & 1};
        if (b < a) { swap(a, b); result.swaps |= 1 << axis; }
        result.key[4 * axis] = a.first; result.key[4 * axis + 1] = a.second;
        result.key[4 * axis + 2] = b.first; result.key[4 * axis + 3] = b.second;
    }
    return result;
}
static string relabel(string tick, int swaps) {
    auto invert = [](char c) { return c == 'F' ? 'B' : c == 'B' ? 'F' : c; };
    for (int axis = 0; axis < 3; ++axis) if (swaps & (1 << axis)) {
        char a = tick[axis], b = tick[axis + 3];
        tick[axis] = invert(b); tick[axis + 3] = invert(a);
    }
    return tick;
}
static void advance(const Case& cs, State& state, const string& tick) {
    for (int e = 0; e < 6; ++e) {
        if (tick[e] == 'F') state.pos[e] = (state.pos[e] + 1) % cs.L;
        else if (tick[e] == 'B') state.pos[e] = (state.pos[e] + cs.L - 1) % cs.L;
        else if (tick[e] == 'T') state.active ^= 1 << e;
    }
}
static Program splice(const Case& cs, const vector<vector<Program>>& sources, int index, int limit) {
    map<array<int, 12>, int> ids;
    map<pair<int, int>, Program> best_edges;
    auto id = [&](const array<int, 12>& key) {
        auto [found, inserted] = ids.emplace(key, ids.size());
        return found->second;
    };
    State initial;
    for (int e = 0; e < 6; ++e) initial.pos[e] = cs.home(e);
    id(canonical(cs, initial).key);
    for (const auto& source : sources) {
        if (source[index].empty()) continue;
        Program program = source[index];
        if (!cs.finish(program)) continue;
        State state = initial;
        B matter = bzero();
        int begin = 0, from = 0, swaps = canonical(cs, initial).swaps;
        for (int t = 0; t < static_cast<int>(program.size()); ++t) {
            advance(cs, state, program[t]);
            B two, three; cs.config(state.pos.data(), state.active, two, three);
            matter = bandn(bor(matter, two), three);
            if (!beq(band(matter, cs.P), cs.P) || !bnone(band(matter, cs.H))) continue;
            matter = bandn(matter, cs.P);
            auto end = canonical(cs, state);
            int to = id(end.key);
            auto edge_key = make_pair(from, to);
            auto found = best_edges.find(edge_key);
            if (found == best_edges.end() || static_cast<int>(found->second.size()) > t + 1 - begin) {
                Program edge;
                for (int j = begin; j <= t; ++j) edge.push_back(relabel(program[j], swaps));
                best_edges[edge_key] = move(edge);
            }
            begin = t + 1; from = to; swaps = end.swaps;
        }
    }
    struct Edge { int from, to; Program ops; };
    vector<Edge> edges;
    for (auto& [key, ops] : best_edges) edges.push_back({key.first, key.second, move(ops)});
    const int inf = INT_MAX / 4;
    vector<vector<int>> dist(cs.req + 1, vector<int>(ids.size(), inf));
    vector<vector<int>> via(cs.req + 1, vector<int>(ids.size(), -1));
    dist[0][0] = 0;
    for (int captures = 0; captures < cs.req; ++captures) for (int i = 0; i < static_cast<int>(edges.size()); ++i) {
        const auto& edge = edges[i];
        int value = dist[captures][edge.from] + edge.ops.size();
        if (value < dist[captures + 1][edge.to]) {
            dist[captures + 1][edge.to] = value; via[captures + 1][edge.to] = i;
        }
    }
    int last = min_element(dist[cs.req].begin(), dist[cs.req].end()) - dist[cs.req].begin();
    if (dist[cs.req][last] > limit) return {};
    vector<int> chain;
    for (int captures = cs.req; captures > 0; --captures) {
        int edge = via[captures][last];
        if (edge < 0) return {};
        chain.push_back(edge); last = edges[edge].from;
    }
    reverse(chain.begin(), chain.end());
    State state = initial; Program result;
    for (int i : chain) {
        int swaps = canonical(cs, state).swaps;
        for (const auto& canonical_tick : edges[i].ops) {
            string tick = relabel(canonical_tick, swaps);
            result.push_back(tick); advance(cs, state, tick);
        }
    }
    if (!cs.finish(result) || static_cast<int>(result.size()) > limit) return {};
    return result;
}
static vector<string> split(const string& input) {
    vector<string> result; stringstream stream(input); string value;
    while (getline(stream, value, ',')) if (!value.empty()) result.push_back(value);
    return result;
}
int main(int argc, char** argv) {
    map<string, string> options{{"names", "0-example,1-small,2-medium,3-large,4-enormous"},
        {"baseline", "cpp/outputs"}, {"sources", ""}, {"outdir", "cpp/candidates/splice_cpp"}, {"threads", "3"}};
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i]; auto equal = arg.find('=');
        if (equal == string::npos || !options.count(arg.substr(0, equal))) { cerr << "Unknown option: " << arg << '\n'; return 1; }
        options[arg.substr(0, equal)] = arg.substr(equal + 1);
    }
    int workers = stoi(options["threads"]);
    if (workers < 1) return 1;
    vector<string> paths = split(options["sources"]);
    if (paths.empty()) {
        paths = {"cpp/outputs", "python/outputs"};
        for (const string parent : {"cpp/candidates", "python/candidates", "cpp/imported_candidates"})
            if (filesystem::exists(parent)) for (const auto& child : filesystem::directory_iterator(parent))
                if (child.is_directory()) paths.push_back(child.path().string());
        sort(paths.begin(), paths.end());
    }
    for (const auto& name : split(options["names"])) {
        auto cases = load_cases("level-6/in_level-6_" + name + ".txt");
        string filename = "out_level-6_" + name + ".txt";
        auto baseline = read_programs(options["baseline"] + "/" + filename, cases.size());
        for (size_t i = 0; i < cases.size(); ++i) if (!cases[i].finish(baseline[i])) return 2;
        vector<vector<Program>> sources{baseline};
        for (const auto& path : paths) if (filesystem::exists(path + "/" + filename))
            sources.push_back(read_programs(path + "/" + filename, cases.size()));
        vector<Program> result(cases.size()); atomic<int> next(0); mutex output_mutex;
        auto worker = [&]() {
            for (;;) {
                int i = next++; if (i >= static_cast<int>(cases.size())) break;
                Program candidate = splice(cases[i], sources, i, baseline[i].size() - 1);
                if (candidate.empty()) continue;
                lock_guard<mutex> guard(output_mutex);
                result[i] = move(candidate);
                write_programs(options["outdir"] + "/" + filename, result);
                fprintf(stderr, "%s case=%d %zu->%zu\n", name.c_str(), i, baseline[i].size(), result[i].size());
            }
        };
        vector<thread> threads;
        for (int i = 0; i < workers; ++i) threads.emplace_back(worker);
        for (auto& t : threads) t.join();
        write_programs(options["outdir"] + "/" + filename, result);
        long before = 0, after = 0;
        for (size_t i = 0; i < cases.size(); ++i) { before += baseline[i].size(); after += result[i].empty() ? baseline[i].size() : result[i].size(); }
        printf("%s: %ld -> %ld (%zu sources)\n", name.c_str(), before, after, sources.size()); fflush(stdout);
    }
}
