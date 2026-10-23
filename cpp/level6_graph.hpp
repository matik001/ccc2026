// Search a graph of capture-boundary configurations, then use dynamic
// programming to select the shortest sequence of exactly req edges.
// Based on the colleague's graph mode, with bounded segment length and
// deduplicated captured configurations from the specialized beam.
static Program graph_search(const Case& cs, const Program& baseline, int width,
                            const vector<Settings>& settings, int max_nodes,
                            int segment_limit, u64 seed) {
    if (cs.req < 2 || baseline.empty()) return {};
    if (!segment_limit) segment_limit = max(4, static_cast<int>(baseline.size()) / cs.req + 5);
    map<array<int, 7>, int> ids;
    vector<Node> nodes;
    struct Edge { int from, to; Program ops; };
    vector<Edge> edges;
    auto id = [&](Node node) {
        array<int, 7> key;
        copy(node.pos, node.pos + 6, key.begin()); key[6] = node.act;
        auto [found, inserted] = ids.emplace(key, nodes.size());
        if (inserted) {
            node.m = bzero(); node.acc = 0;
            nodes.push_back(node);
        }
        return found->second;
    };
    Node state = state_after(cs, {});
    id(state);
    int from = 0, begin = 0;
    for (int t = 0; t < static_cast<int>(baseline.size()); ++t) {
        const auto& tick = baseline[t];
        for (int e = 0; e < 6; ++e) {
            if (tick[e] == 'F') state.pos[e] = (state.pos[e] + 1) % cs.L;
            else if (tick[e] == 'B') state.pos[e] = (state.pos[e] + cs.L - 1) % cs.L;
            else if (tick[e] == 'T') state.act ^= 1 << e;
        }
        B two, three; cs.config(state.pos, state.act, two, three);
        state.m = bandn(bor(state.m, two), three);
        if (!beq(band(state.m, cs.P), cs.P) || !bnone(band(state.m, cs.H))) continue;
        state.m = bandn(state.m, cs.P);
        int to = id(state);
        edges.push_back({from, to, Program(baseline.begin() + begin, baseline.begin() + t + 1)});
        begin = t + 1; from = to;
    }
    for (int node = 0; node < min(max_nodes, static_cast<int>(nodes.size())); ++node) {
        for (size_t s = 0; s < settings.size(); ++s) {
            Node initial = nodes[node];
            CaptureCollection found;
            beam(cs, width, settings[s], segment_limit, seed + node * 7919 + s * 101, &initial, {}, &found);
            for (auto& [ops, end] : found.paths) {
                int to = id(end);
                edges.push_back({node, to, move(ops)});
            }
        }
    }
    const int inf = INT_MAX / 4;
    vector<vector<int>> dist(cs.req + 1, vector<int>(nodes.size(), inf));
    vector<vector<int>> via(cs.req + 1, vector<int>(nodes.size(), -1));
    dist[0][0] = 0;
    for (int captures = 0; captures < cs.req; ++captures) {
        for (int i = 0; i < static_cast<int>(edges.size()); ++i) {
            const auto& edge = edges[i];
            int length = dist[captures][edge.from] + edge.ops.size();
            if (length < dist[captures + 1][edge.to]) {
                dist[captures + 1][edge.to] = length; via[captures + 1][edge.to] = i;
            }
        }
    }
    int last = min_element(dist[cs.req].begin(), dist[cs.req].end()) - dist[cs.req].begin();
    if (dist[cs.req][last] >= static_cast<int>(baseline.size())) return {};
    vector<int> chain;
    for (int captures = cs.req; captures > 0; --captures) {
        int edge = via[captures][last];
        if (edge < 0) return {};
        chain.push_back(edge); last = edges[edge].from;
    }
    reverse(chain.begin(), chain.end());
    Program result;
    for (int edge : chain) result.insert(result.end(), edges[edge].ops.begin(), edges[edge].ops.end());
    if (!cs.finish(result) || result.size() >= baseline.size()) return {};
    return result;
}
