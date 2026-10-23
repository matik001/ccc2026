// Search one capture backwards. Last writes remove obligations; a write of the
// wrong value makes the predecessor impossible. Capture boundaries have P=H=0,
// so a shorter path with the same emitter endpoints can replace an inner cycle.
template<int NW>
static Program reverse_segment_impl(const Case& cs, const Node& begin, const Node& end,
                                    int limit, int width, Rng& rng, bool free_end) {
    using BB = SmallB<NW>;
    if (limit < 1) return {};
    struct ReverseNode {
        array<int, 6> pos{};
        int active = 0, parent = -1;
        BB ones{}, zeros{};
        string ops = "------";
    };
    struct Option { int pos, active; char op; BB mask; };
    struct Axis { int i, j; BB one, two; };
    struct Candidate { double score; int parent; array<int, 6> choices; BB ones, zeros; };
    const BB relevant = smallBits<NW>(bor(cs.P, cs.H));
    const BB initial_matter = band(smallBits<NW>(begin.m), relevant);
    vector<BB> rays[6];
    for (int e = 0; e < 6; ++e)
        for (const auto& mask : cs.ray[e]) rays[e].push_back(smallBits<NW>(mask));
    BB target = smallBits<NW>(cs.P), halo = smallBits<NW>(cs.H);
    vector<pair<double, ReverseNode>> roots;
    auto add_root = [&](const array<int, 6>& positions, int active) {
        B root_two, root_three;
        cs.config(positions.data(), active, root_two, root_three);
        BB two = smallBits<NW>(root_two), three = smallBits<NW>(root_three);
        if (!bnone(band(target, three)) || !bnone(band(halo, bandn(two, three)))) return;
        ReverseNode root;
        root.pos = positions; root.active = active;
        root.ones = bandn(target, two); root.zeros = bandn(halo, three);
        int distance = 0, sum_distance = 0;
        for (int e = 0; e < 6; ++e) {
            int d = abs(cs.delta(positions[e], begin.pos[e])) + (((active >> e) & 1) != ((begin.act >> e) & 1));
            distance = max(distance, d); sum_distance += d;
        }
        if (distance > limit) return;
        double score = -4.0 * pc(bandn(root.ones, initial_matter)) - 0.35 * pc(root.zeros) -
                       1.5 * distance - 0.1 * sum_distance + 0.8 * rng.uni();
        roots.push_back({score, move(root)});
    };
    array<int, 6> positions;
    copy(end.pos, end.pos + 6, positions.begin());
    if (!free_end) add_root(positions, end.act);
    else {
        auto generate = [&](auto&& self, int emitter, int active) -> void {
            if (emitter == 6) { add_root(positions, active); return; }
            int p = end.pos[emitter];
            positions[emitter] = p; self(self, emitter + 1, active);
            self(self, emitter + 1, active ^ (1 << emitter));
            for (int delta : {-1, 1}) {
                int next = (p + delta + cs.L) % cs.L;
                if (cs.allowed[emitter][next]) { positions[emitter] = next; self(self, emitter + 1, active); }
            }
            positions[emitter] = p;
        };
        generate(generate, 0, end.act);
    }
    if (roots.empty()) return {};
    sort(roots.begin(), roots.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    vector<ReverseNode> first_layer;
    for (auto& root : roots) {
        first_layer.push_back(move(root.second));
        if (static_cast<int>(first_layer.size()) == width) break;
    }
    vector<vector<ReverseNode>> layers;
    layers.push_back(move(first_layer));
    for (int depth = 1; depth <= limit; ++depth) {
        const auto& layer = layers.back();
        vector<array<vector<Option>, 6>> options(layer.size());
        vector<Candidate> pool, local;
        for (int pi = 0; pi < static_cast<int>(layer.size()); ++pi) {
            const auto& node = layer[pi];
            auto& opts = options[pi];
            int distances[6][4];
            bool can_finish = bnone(bandn(node.ones, initial_matter)) &&
                              bnone(band(node.zeros, initial_matter));
            int finish_choices[6];
            for (int e = 0; e < 6; ++e) {
                int p = node.pos[e], a = (node.active >> e) & 1;
                finish_choices[e] = -1;
                auto add = [&](int previous, int active, char op) {
                    int idx = opts[e].size();
                    distances[e][idx] = abs(cs.delta(previous, begin.pos[e])) + (active != ((begin.act >> e) & 1));
                    if (distances[e][idx] == 0) finish_choices[e] = idx;
                    opts[e].push_back({previous, active, op, active ? rays[e][previous] : BB{}});
                };
                add(p, a, '-');
                int back = (p + cs.L - 1) % cs.L, forward = (p + 1) % cs.L;
                if (cs.allowed[e][back] || back == begin.pos[e]) add(back, a, 'F');
                if (cs.allowed[e][forward] || forward == begin.pos[e]) add(forward, a, 'B');
                add(p, !a, 'T');
                can_finish &= finish_choices[e] >= 0;
            }
            if (can_finish) {
                Program result;
                string tick(6, '-');
                for (int e = 0; e < 6; ++e) tick[e] = opts[e][finish_choices[e]].op;
                result.push_back(tick);
                int index = pi;
                for (int d = depth - 1; d > 0; --d) {
                    const auto& saved = layers[d][index];
                    result.push_back(saved.ops);
                    index = saved.parent;
                }
                return result;
            }
            if (depth == limit) continue;
            Axis axes[3][16]; int counts[3]{};
            for (int axis = 0; axis < 3; ++axis)
                for (int i = 0; i < static_cast<int>(opts[axis].size()); ++i)
                    for (int j = 0; j < static_cast<int>(opts[axis + 3].size()); ++j) {
                        if (distances[axis][i] > limit - depth || distances[axis + 3][j] > limit - depth) continue;
                        const BB& a = opts[axis][i].mask; const BB& b = opts[axis + 3][j].mask;
                        axes[axis][counts[axis]++] = {i, j, bxor(a, b), band(a, b)};
                    }
            local.clear();
            for (int i = 0; i < counts[0]; ++i) for (int j = 0; j < counts[1]; ++j) {
                const auto& a = axes[0][i]; const auto& b = axes[1][j];
                BB aa = bor(a.one, a.two), bb = bor(b.one, b.two);
                BB one = bandn(bxor(a.one, b.one), bor(a.two, b.two));
                BB two = bor(bor(bandn(a.two, bb), bandn(b.two, aa)), band(a.one, b.one));
                BB three = bor(band(a.two, bb), band(b.two, aa));
                BB any = bor(bor(one, two), three);
                if (!bnone(band(three, node.ones))) continue;
                for (int k = 0; k < counts[2]; ++k) {
                    const auto& c = axes[2][k];
                    BB cc = bor(c.one, c.two);
                    BB exact = bor(bor(bandn(two, cc), band(one, c.one)), bandn(c.two, any));
                    BB destroy = bor(bor(three, band(two, cc)), band(one, c.two));
                    if (!bnone(band(exact, node.zeros)) || !bnone(band(destroy, node.ones))) continue;
                    Candidate cand;
                    cand.parent = pi;
                    cand.choices = {a.i, b.i, c.i, a.j, b.j, c.j};
                    cand.ones = bandn(node.ones, exact);
                    cand.zeros = bandn(node.zeros, destroy);
                    int max_distance = 0, sum_distance = 0;
                    for (int e = 0; e < 6; ++e) {
                        int d = distances[e][cand.choices[e]];
                        max_distance = max(max_distance, d); sum_distance += d;
                    }
                    cand.score = -4.0 * pc(bandn(cand.ones, initial_matter)) -
                                 1.0 * pc(band(cand.zeros, initial_matter)) -
                                 0.35 * pc(cand.zeros) - 1.5 * max_distance -
                                 0.1 * sum_distance + 0.8 * rng.uni();
                    local.push_back(cand);
                }
            }
            if (static_cast<int>(local.size()) > width) {
                nth_element(local.begin(), local.begin() + width, local.end(),
                            [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
                local.resize(width);
            }
            pool.insert(pool.end(), local.begin(), local.end());
        }
        sort(pool.begin(), pool.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
        vector<ReverseNode> next;
        unordered_set<u64> seen;
        for (const auto& cand : pool) {
            ReverseNode node;
            node.ones = cand.ones; node.zeros = cand.zeros; node.parent = cand.parent;
            u64 hash = 1469598103934665603ULL;
            auto mix = [&](u64 value) { hash ^= value; hash *= 1099511628211ULL; };
            for (int e = 0; e < 6; ++e) {
                const auto& op = options[cand.parent][e][cand.choices[e]];
                node.pos[e] = op.pos; node.active |= op.active << e; node.ops[e] = op.op;
                mix(op.pos); mix(op.active);
            }
            for (int w = 0; w < NW; ++w) { mix(node.ones.w[w]); mix(node.zeros.w[w]); }
            if (!seen.insert(hash).second) continue;
            next.push_back(move(node));
            if (static_cast<int>(next.size()) == width) break;
        }
        if (next.empty()) return {};
        layers.push_back(move(next));
    }
    return {};
}

static Program reverse_segment(const Case& cs, const Node& begin, const Node& end,
                               int limit, int width, Rng& rng, bool free_end = false) {
    switch ((3 * cs.n * (cs.n - 1) + 64) / 64) {
        case 1: return reverse_segment_impl<1>(cs, begin, end, limit, width, rng, free_end);
        case 2: return reverse_segment_impl<2>(cs, begin, end, limit, width, rng, free_end);
        case 3: return reverse_segment_impl<3>(cs, begin, end, limit, width, rng, free_end);
        case 4: return reverse_segment_impl<4>(cs, begin, end, limit, width, rng, free_end);
        case 5: return reverse_segment_impl<5>(cs, begin, end, limit, width, rng, free_end);
        case 6: return reverse_segment_impl<6>(cs, begin, end, limit, width, rng, free_end);
        default: return reverse_segment_impl<7>(cs, begin, end, limit, width, rng, free_end);
    }
}

static Program reverse_polish(const Case& cs, Program program, int width, Rng& rng, bool free_end = false) {
    const size_t old_size = program.size();
    unordered_map<string, Program> cache;
    for (;;) {
        int previous_tick = 0;
        Node previous = state_after(cs, {});
        bool improved = false;
        for (int end_tick = 1; end_tick <= static_cast<int>(program.size()); ++end_tick) {
            Node end = state_after(cs, Program(program.begin(), program.begin() + end_tick));
            if (end.acc == previous.acc) continue;
            if (free_end && end.acc < cs.req) { previous_tick = end_tick; previous = end; continue; }
            int length = end_tick - previous_tick;
            string key = to_string(length) + ":" + to_string(previous.act) + ":" + to_string(end.act);
            for (int e = 0; e < 6; ++e) key += ":" + to_string(previous.pos[e]) + ":" + to_string(end.pos[e]);
            auto entry = cache.find(key);
            if (entry == cache.end()) {
                Program path = reverse_segment(cs, previous, end, length - 1, width, rng, free_end);
                entry = cache.emplace(key, move(path)).first;
            }
            if (!entry->second.empty()) {
                Program candidate(program.begin(), program.begin() + previous_tick);
                candidate.insert(candidate.end(), entry->second.begin(), entry->second.end());
                candidate.insert(candidate.end(), program.begin() + end_tick, program.end());
                if (cs.finish(candidate) && candidate.size() < program.size()) {
                    program = move(candidate); improved = true; break;
                }
            }
            previous_tick = end_tick; previous = end;
        }
        if (!improved) break;
    }
    return program.size() < old_size ? program : Program{};
}
