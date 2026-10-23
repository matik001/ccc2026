// Level 6: solvery w C++ (port level6_beam.py, level6_cycles.py, level6_polish.py).
//
// Budowanie:  g++ -O2 -march=native -pthread -o level6_opt level6_opt.cpp
// Uruchamianie z katalogu python/:
//   ./level6_opt beam   names=3-large,4-enormous width=100 settings=3:0.3:2,12:1:3 tag=_c1
//   ./level6_opt cycles names=2-medium ks=1,2,3,4,5,6,8 iterations=20000 restarts=3 tag=_c1
//   ./level6_opt polish names=2-medium iterations=20000 tag=_c1
// Baseline to outputs/out_level-6_<name>.txt; kandydaci trafiaja do
// candidates/<tryb><tag>/out_level-6_<name>.txt (NONE = brak poprawy),
// a level6_merge.py wybiera najkrotsze programy.

#include <bits/stdc++.h>
using namespace std;
typedef uint64_t u64;

// ---------------------------------------------------------------- bitset
const int WD = 7;  // do 448 komorek (n=12 ma 397)
struct B {
    u64 w[WD];
};
static inline B bzero() { B r; memset(r.w, 0, sizeof r.w); return r; }
#define BOP(name, expr) \
    static inline B name(const B& a, const B& b) { B r; for (int i = 0; i < WD; i++) r.w[i] = (expr); return r; }
BOP(bor, a.w[i] | b.w[i])
BOP(band, a.w[i] & b.w[i])
BOP(bxor, a.w[i] ^ b.w[i])
BOP(bandn, a.w[i] & ~b.w[i])  // a & ~b
static inline int pc(const B& a) { int s = 0; for (int i = 0; i < WD; i++) s += __builtin_popcountll(a.w[i]); return s; }
static inline bool bnone(const B& a) { u64 s = 0; for (int i = 0; i < WD; i++) s |= a.w[i]; return s == 0; }
static inline bool beq(const B& a, const B& b) { return memcmp(a.w, b.w, sizeof a.w) == 0; }
static inline void bset(B& a, int i) { a.w[i >> 6] |= 1ULL << (i & 63); }

// ---------------------------------------------------------------- rng
struct Rng {
    u64 s;
    explicit Rng(u64 seed) : s(seed * 0x9E3779B97F4A7C15ULL + 12345) {}
    u64 next() { u64 z = (s += 0x9E3779B97F4A7C15ULL); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; return z ^ (z >> 31); }
    double uni() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    int below(int n) { return (int)(next() % (u64)n); }
};

// ---------------------------------------------------------------- przypadek
typedef vector<string> Program;  // tick = 6 znakow F/B/T/-
static int g_slack = 0;          // 1: beam moze zjechac na narozniki (pusty promien = wylaczenie ruchem)
static double g_noise = 1.0;     // amplituda losowego rozbijania remisow w beamie
const int DR[6] = {0, 1, 1, 0, -1, -1};
const int DC[6] = {2, 1, -1, -2, -1, 1};

struct Case {
    int n, L, req, pcount;
    vector<string> board;
    B P, H;
    vector<B> ray[6];
    vector<char> useful[6];
    vector<char> allowed[6];  // pozycje dostepne dla beamu

    int home(int e) const { return e * n; }
    int delta(int a, int b) const { int d = ((b - a) % L + L) % L; return d <= L / 2 ? d : d - L; }

    void build() {
        L = 6 * n;
        int h = board.size(), w = board[0].size();
        map<pair<int, int>, int> idx;
        for (int r = 0; r < h; r++)
            for (int c = 0; c < w; c++)
                if (board[r][c] == 'O' || board[r][c] == 'M') { int k = idx.size(); idx[{r, c}] = k; }
        if ((int)idx.size() > WD * 64) { fprintf(stderr, "plansza za duza\n"); exit(1); }
        P = bzero(); H = bzero();
        for (auto& [rc, i] : idx) {
            auto [r, c] = rc;
            if (board[r][c] == 'M') bset(P, i);
            else {
                for (int d = 0; d < 6; d++) {
                    auto it = idx.find({r + DR[d], c + DC[d]});
                    if (it != idx.end() && board[r + DR[d]][c + DC[d]] == 'M') { bset(H, i); break; }
                }
            }
        }
        pcount = pc(P);
        // obwod jak level4.border_positions
        vector<pair<int, int>> border;
        int r = n, c = 0;
        const int BD[6][2] = {{-1, 1}, {0, 2}, {1, 1}, {1, -1}, {0, -2}, {-1, -1}};
        for (int s = 0; s < 6; s++)
            for (int i = 0; i < n; i++) { border.push_back({r, c}); r += BD[s][0]; c += BD[s][1]; }
        for (int e = 0; e < 6; e++) {
            ray[e].assign(L, bzero());
            useful[e].assign(L, 0);
            for (int p = 0; p < L; p++) {
                int rr = border[p].first + DR[e], cc = border[p].second + DC[e];
                while (rr >= 0 && rr < h && cc >= 0 && cc < w) {
                    auto it = idx.find({rr, cc});
                    if (it != idx.end()) bset(ray[e][p], it->second);
                    rr += DR[e]; cc += DC[e];
                }
                useful[e][p] = !bnone(ray[e][p]);
            }
            allowed[e].assign(L, 0);
            for (int p = 0; p < L; p++)
                allowed[e][p] = useful[e][p] || (g_slack && abs(delta(home(e), p)) <= n);
        }
    }

    // (dokladnie 2 trafienia, >=3 trafienia)
    void config(const int* pos, int active, B& two, B& three) const {
        B one = bzero(); two = bzero(); three = bzero();
        for (int e = 0; e < 6; e++) {
            if (!(active >> e & 1)) continue;
            const B& m = ray[e][pos[e]];
            for (int i = 0; i < WD; i++) {
                u64 o = one.w[i], t = two.w[i], h = three.w[i], x = m.w[i];
                three.w[i] = h | (t & x);
                two.w[i] = (t & ~x) | (o & x);
                one.w[i] = (o & ~x) | (x & ~(o | t | h));
            }
        }
    }

    // dlugosc programu do req-tego odbioru albo -1
    int run(const Program& prog) const {
        int pos[6], active = 0, acc = 0;
        for (int e = 0; e < 6; e++) pos[e] = home(e);
        B m = bzero(), two, three;
        for (size_t t = 0; t < prog.size(); t++) {
            for (int e = 0; e < 6; e++) {
                char op = prog[t][e];
                if (op == 'F') pos[e] = (pos[e] + 1) % L;
                else if (op == 'B') pos[e] = (pos[e] + L - 1) % L;
                else if (op == 'T') active ^= 1 << e;
            }
            if (active) { config(pos, active, two, three); m = bandn(bor(m, two), three); }
            if (beq(band(m, P), P) && bnone(band(m, H))) {
                acc++;
                m = bandn(m, P);
                if (acc == req) return t + 1;
            }
        }
        return -1;
    }

    bool finish(Program& prog) const {
        int len = run(prog);
        if (len < 0) return false;
        prog.resize(len);
        return true;
    }
};

// ---------------------------------------------------------------- I/O
static string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == string::npos ? "" : s.substr(a, b - a + 1);
}

static vector<Case> load_cases(const string& path) {
    ifstream f(path);
    if (!f) { fprintf(stderr, "brak %s\n", path.c_str()); exit(1); }
    vector<string> lines;
    string line;
    while (getline(f, line)) { line = trim(line); if (!line.empty()) lines.push_back(line); }
    size_t i = 0;
    int count = stoi(lines[i++]);
    vector<Case> cases(count);
    for (auto& c : cases) {
        int h, w;
        sscanf(lines[i++].c_str(), "%d %d %d %d", &c.n, &h, &w, &c.req);
        for (int r = 0; r < h; r++) c.board.push_back(lines[i++]);
        c.build();
    }
    return cases;
}

static vector<Program> read_programs(const string& path, size_t count) {
    vector<Program> result;
    ifstream f(path);
    if (!f) return vector<Program>(count);
    Program cur;
    bool in_block = false;
    string line;
    auto flush = [&]() { if (in_block) result.push_back(cur); cur.clear(); in_block = false; };
    while (getline(f, line)) {
        line = trim(line);
        if (line.empty()) { flush(); continue; }
        in_block = true;
        if (line == "NONE") continue;
        istringstream ss(line);
        string tick, tok;
        while (ss >> tok) tick += tok;
        cur.push_back(tick);
    }
    flush();
    if (result.size() != count) return vector<Program>(count);
    return result;
}

static void write_programs(const string& path, const vector<Program>& progs) {
    filesystem::create_directories(filesystem::path(path).parent_path());
    ofstream f(path);
    for (size_t i = 0; i < progs.size(); i++) {
        if (i) f << "\n";
        if (progs[i].empty()) f << "NONE\n";
        for (auto& t : progs[i]) {
            for (int e = 0; e < 6; e++) f << t[e] << (e < 5 ? ' ' : '\n');
        }
    }
}

// ---------------------------------------------------------------- beam (T4)
struct Settings { double alpha, gamma, beta; };

struct Node {
    int pos[6];
    int act, acc;
    B m;
    int parent;
    char ops[7];
    double score;
};

struct Opt { char op; int pos; int act; B mask; };

static u64 node_hash(const Node& x) {
    u64 h = 1469598103934665603ULL;
    auto mix = [&](u64 v) { h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2); h *= 1099511628211ULL; };
    for (int e = 0; e < 6; e++) mix(x.pos[e]);
    mix(x.act); mix(x.acc);
    for (int i = 0; i < WD; i++) mix(x.m.w[i]);
    return h;
}

// Stan po wykonaniu prefiksu programu (do startu beamu w srodku programu).
static Node state_after(const Case& cs, const Program& prefix) {
    Node nd;
    for (int e = 0; e < 6; e++) nd.pos[e] = cs.home(e);
    nd.act = 0; nd.acc = 0; nd.m = bzero(); nd.parent = -1; nd.score = 0;
    strcpy(nd.ops, "------");
    B two, three;
    for (auto& tick : prefix) {
        for (int e = 0; e < 6; e++) {
            char op = tick[e];
            if (op == 'F') nd.pos[e] = (nd.pos[e] + 1) % cs.L;
            else if (op == 'B') nd.pos[e] = (nd.pos[e] + cs.L - 1) % cs.L;
            else if (op == 'T') nd.act ^= 1 << e;
        }
        if (nd.act) { cs.config(nd.pos, nd.act, two, three); nd.m = bandn(bor(nd.m, two), three); }
        if (beq(band(nd.m, cs.P), cs.P) && bnone(band(nd.m, cs.H))) { nd.acc++; nd.m = bandn(nd.m, cs.P); }
    }
    return nd;
}

// Zwraca same ticki wygenerowane przez beam (bez prefiksu); dlugosc <= limit.
static Program beam(const Case& cs, int width, Settings st, int limit, u64 seed, const Node* from = nullptr,
                    const Program& prefix = {}) {
    Rng rng(seed);
    vector<vector<Node>> layers;
    Node start = from ? *from : state_after(cs, {});
    start.parent = -1;
    layers.push_back({start});
    struct Child { double score; int parent, i01, i2; B m; int acc; };
    struct AxisC { int i, j; B one, two; double pot; };
    struct PairC { int a, b; B s1, s2, g3, any; double pot; };
    vector<Child> pool, local;
    for (int depth = 0; depth < limit; depth++) {
        const vector<Node>& layer = layers.back();
        pool.clear();
        // dziecko z ukonczonym zadaniem: (rodzic, i01, i2)
        int done_parent = -1, done01 = -1, done2 = -1;
        vector<array<vector<Opt>, 6>> all_opts(layer.size());
        vector<vector<PairC>> all_pairs(layer.size());
        vector<vector<AxisC>> all_third(layer.size());
        for (size_t pi = 0; pi < layer.size() && done_parent < 0; pi++) {
            const Node& nd = layer[pi];
            B missing = bandn(cs.P, nd.m), dirty = band(cs.H, nd.m);
            auto& opts = all_opts[pi];
            double pots[6][4];
            for (int e = 0; e < 6; e++) {
                int p = nd.pos[e], a = nd.act >> e & 1;
                auto add = [&](char op, int np, int na) {
                    const B& r = cs.ray[e][np];
                    pots[e][opts[e].size()] = pc(band(r, missing)) + st.beta * pc(band(r, dirty));
                    opts[e].push_back({op, np, na, na ? r : bzero()});
                };
                add('-', p, a);
                int f = (p + 1) % cs.L, b = (p + cs.L - 1) % cs.L;
                if (cs.allowed[e][f]) add('F', f, a);
                if (cs.allowed[e][b]) add('B', b, a);
                add('T', p, !a);
            }
            AxisC ax[3][16];
            int axn[3];
            for (int a = 0; a < 3; a++) {
                axn[a] = 0;
                for (int i = 0; i < (int)opts[a].size(); i++)
                    for (int j = 0; j < (int)opts[a + 3].size(); j++) {
                        const B& x = opts[a][i].mask; const B& y = opts[a + 3][j].mask;
                        ax[a][axn[a]++] = {i, j, bxor(x, y), band(x, y), pots[a][i] + pots[a + 3][j]};
                    }
            }
            auto& pairs = all_pairs[pi];
            for (int u = 0; u < axn[0]; u++)
                for (int v = 0; v < axn[1]; v++) {
                    const AxisC& A = ax[0][u]; const AxisC& C = ax[1][v];
                    B u0 = bor(A.one, A.two), u1 = bor(C.one, C.two);
                    PairC pcand;
                    pcand.a = u; pcand.b = v;
                    pcand.s1 = bandn(bxor(A.one, C.one), bor(A.two, C.two));
                    pcand.s2 = bor(bor(bandn(A.two, u1), bandn(C.two, u0)), band(A.one, C.one));
                    pcand.g3 = bor(band(A.two, u1), band(C.two, u0));
                    pcand.any = bor(bor(pcand.s1, pcand.s2), pcand.g3);
                    pcand.pot = A.pot + C.pot;
                    pairs.push_back(pcand);
                }
            auto& third = all_third[pi];
            third.assign(ax[2], ax[2] + axn[2]);
            // ponizej indeksy par wskazuja na ax[0], ax[1] - zapamietujemy je w Node przy materializacji
            local.clear();
            for (int u = 0; u < (int)pairs.size(); u++) {
                const PairC& Q = pairs[u];
                for (int v = 0; v < (int)third.size(); v++) {
                    const AxisC& T = third[v];
                    Child ch;
                    B u2 = bor(T.one, T.two);
                    B exact2 = bor(bor(bandn(Q.s2, u2), band(Q.s1, T.one)), bandn(T.two, Q.any));
                    B ge3 = bor(bor(Q.g3, band(Q.s2, u2)), band(Q.s1, T.two));
                    ch.m = bandn(bor(nd.m, exact2), ge3);
                    ch.acc = nd.acc;
                    if (beq(band(ch.m, cs.P), cs.P) && bnone(band(ch.m, cs.H))) {
                        ch.acc++;
                        ch.m = bandn(ch.m, cs.P);
                        if (ch.acc == cs.req) { done_parent = pi; done01 = u; done2 = v; break; }
                    }
                    ch.score = ch.acc * 1e6 + 4.0 * pc(band(ch.m, cs.P)) - 4.0 * st.alpha * pc(band(ch.m, cs.H)) +
                               st.gamma * (Q.pot + T.pot) + g_noise * rng.uni();
                    ch.parent = pi; ch.i01 = u; ch.i2 = v;
                    local.push_back(ch);
                }
                if (done_parent >= 0) break;
            }
            if (done_parent >= 0) break;
            if ((int)local.size() > width) {
                nth_element(local.begin(), local.begin() + width, local.end(),
                            [](const Child& x, const Child& y) { return x.score > y.score; });
                local.resize(width);
            }
            pool.insert(pool.end(), local.begin(), local.end());
        }
        auto materialize = [&](int pi, int u, int v, Node& out) {
            const Node& nd = layer[pi];
            auto& opts = all_opts[pi];
            // odtworzenie indeksow opcji: pary (a,a+3) w kolejnosci tworzenia ax[a]
            int axn0 = opts[3].size(), axn1 = opts[4].size(), axn2 = opts[5].size();
            int A = all_pairs[pi][u].a, C = all_pairs[pi][u].b;
            int idx[6];
            idx[0] = A / axn0; idx[3] = A % axn0;
            idx[1] = C / axn1; idx[4] = C % axn1;
            idx[2] = all_third[pi][v].i; idx[5] = all_third[pi][v].j;
            (void)axn2;
            out.act = 0;
            for (int e = 0; e < 6; e++) {
                const Opt& o = opts[e][idx[e]];
                out.pos[e] = o.pos;
                if (o.act) out.act |= 1 << e;
                out.ops[e] = o.op;
            }
            out.ops[6] = 0;
            out.parent = pi;
            (void)nd;
        };
        if (done_parent >= 0) {
            Node last;
            materialize(done_parent, done01, done2, last);
            Program prog;
            prog.push_back(string(last.ops));
            int pi = done_parent;
            for (int d = (int)layers.size() - 1; d >= 1; d--) {
                const Node& nd = layers[d][pi];
                prog.push_back(string(nd.ops));
                pi = nd.parent;
            }
            reverse(prog.begin(), prog.end());
            Program full = prefix;
            full.insert(full.end(), prog.begin(), prog.end());
            if (cs.run(full) == (int)full.size()) return prog;
            return {};
        }
        sort(pool.begin(), pool.end(), [](const Child& x, const Child& y) { return x.score > y.score; });
        vector<Node> next;
        unordered_set<u64> seen;
        for (auto& ch : pool) {
            if ((int)next.size() >= width) break;
            Node nd;
            materialize(ch.parent, ch.i01, ch.i2, nd);
            nd.m = ch.m; nd.acc = ch.acc; nd.score = ch.score;
            if (!seen.insert(node_hash(nd)).second) continue;
            next.push_back(nd);
        }
        if (next.empty()) return {};
        layers.push_back(move(next));
    }
    return {};
}

// ---------------------------------------------------------------- cykle (T1)
static vector<string> all_sequences(int k) {
    vector<string> out;
    if (k > 6) return out;
    int total = 1;
    for (int i = 0; i < k; i++) total *= 4;
    for (int x = 0; x < total; x++) {
        string s; int y = x, f = 0, b = 0, t = 0;
        for (int i = 0; i < k; i++) { char c = "FBT-"[y % 4]; y /= 4; s += c; f += c == 'F'; b += c == 'B'; t += c == 'T'; }
        if (f == b && t % 2 == 0) out.push_back(s);
    }
    return out;
}

static string random_sequence(int k, Rng& rng) {
    string ops(k, '-');
    for (auto& c : ops) c = "FB-T-"[rng.below(5)];
    while (true) {
        int f = count(ops.begin(), ops.end(), 'F'), b = count(ops.begin(), ops.end(), 'B');
        if (f == b) break;
        char extra = f > b ? 'F' : 'B';
        vector<int> where;
        for (int i = 0; i < k; i++) if (ops[i] == extra) where.push_back(i);
        ops[where[rng.below(where.size())]] = rng.below(2) ? '-' : (extra == 'F' ? 'B' : 'F');
    }
    if (count(ops.begin(), ops.end(), 'T') % 2) {
        vector<int> where;
        for (int i = 0; i < k; i++) if (ops[i] == 'T') where.push_back(i);
        ops[where[rng.below(where.size())]] = '-';
    }
    return ops;
}

struct CycleModel {
    const Case& cs;
    int k, cap;
    vector<string> seqs;
    CycleModel(const Case& c, int k_, int cap_) : cs(c), k(k_), cap(cap_), seqs(all_sequences(k_)) {}

    string random_seq(Rng& rng) const { return seqs.empty() ? random_sequence(k, rng) : seqs[rng.below(seqs.size())]; }

    int prefix_length(const int* s, const int* a) const {
        int T0 = 0;
        for (int e = 0; e < 6; e++) T0 = max(T0, abs(cs.delta(cs.home(e), s[e])) + a[e]);
        return T0;
    }

    int evaluate(const int* s, const int* a, const string* q) const {
        vector<B> c2(k), c3(k);
        int pos[6], act = 0;
        for (int e = 0; e < 6; e++) { pos[e] = s[e]; if (a[e]) act |= 1 << e; }
        for (int j = 0; j < k; j++) {
            cs.config(pos, act, c2[j], c3[j]);
            for (int e = 0; e < 6; e++) {
                char op = q[e][j];
                if (op == 'T') act ^= 1 << e;
                else if (op == 'F') pos[e] = (pos[e] + 1) % cs.L;
                else if (op == 'B') pos[e] = (pos[e] + cs.L - 1) % cs.L;
            }
        }
        int t = prefix_length(s, a), j = 0, acc = 0, best_def = cs.pcount;
        if (t == 0) j = 1 % k; else t--;
        B m = bzero();
        while (t < cap) {
            t++;
            m = bandn(bor(m, c2[j]), c3[j]);
            if (++j == k) j = 0;
            B missing = bandn(cs.P, m), bad = band(cs.H, m);
            if (bnone(missing) && bnone(bad)) {
                acc++;
                m = bandn(m, cs.P);
                best_def = cs.pcount;
                if (acc == cs.req) return t;
            } else {
                best_def = min(best_def, pc(missing) + 2 * pc(bad));
            }
        }
        return cap + 30 * (cs.req - acc) + best_def;
    }

    Program program(const int* s, const int* a, const string* q) const {
        int T0 = prefix_length(s, a);
        Program prog;
        for (int t = 1; t <= T0; t++) {
            string tick(6, '-');
            for (int e = 0; e < 6; e++) {
                int d = cs.delta(cs.home(e), s[e]);
                int st = T0 - abs(d) - a[e] + 1;
                if (st <= t && t < st + abs(d)) tick[e] = d > 0 ? 'F' : 'B';
                else if (a[e] && t == T0) tick[e] = 'T';
            }
            prog.push_back(tick);
        }
        while ((int)prog.size() < cap + k)
            for (int j = 0; j < k; j++) {
                string tick(6, '-');
                for (int e = 0; e < 6; e++) tick[e] = q[e][j];
                prog.push_back(tick);
            }
        if (!cs.finish(prog)) return {};
        return prog;
    }
};

static Program anneal_cycles(const CycleModel& model, Rng& rng, int iterations, int& out_cost) {
    const Case& cs = model.cs;
    int span = cs.n;
    auto random_pos = [&](int e) { return ((cs.home(e) + rng.below(2 * span - 1) - (span - 1)) % cs.L + cs.L) % cs.L; };
    int s[6], a[6];
    string q[6];
    string idle(model.k, '-');
    for (int e = 0; e < 6; e++) {
        s[e] = random_pos(e);
        a[e] = rng.uni() < 0.7;
        q[e] = rng.uni() < 0.5 ? model.random_seq(rng) : idle;
    }
    int cost = model.evaluate(s, a, q);
    int bs[6], ba[6];
    string bq[6];
    int best = cost;
    copy(s, s + 6, bs); copy(a, a + 6, ba); copy(q, q + 6, bq);
    double t0 = 4.0, t1 = 0.2;
    for (int it = 0; it < iterations; it++) {
        double temp = t0 * pow(t1 / t0, (double)it / iterations);
        int e = rng.below(6);
        int os = s[e], oa = a[e];
        string oq = q[e];
        double r = rng.uni();
        if (r < 0.25) q[e] = model.random_seq(rng);
        else if (r < 0.35) { int i = rng.below(model.k), j = rng.below(model.k); swap(q[e][i], q[e][j]); }
        else if (r < 0.6) s[e] = (s[e] + (rng.below(2) ? 1 : cs.L - 1)) % cs.L;
        else if (r < 0.72) a[e] = !a[e];
        else if (r < 0.82) s[e] = random_pos(e);
        else if (r < 0.9) q[e] = idle;
        else {
            char op = q[e][0];
            if (op == 'T') a[e] = !a[e];
            else if (op == 'F') s[e] = (s[e] + 1) % cs.L;
            else if (op == 'B') s[e] = (s[e] + cs.L - 1) % cs.L;
            q[e] = q[e].substr(1) + q[e][0];
        }
        int nc = model.evaluate(s, a, q);
        if (nc <= cost || rng.uni() < exp((cost - nc) / temp)) {
            cost = nc;
            if (cost < best) { best = cost; copy(s, s + 6, bs); copy(a, a + 6, ba); copy(q, q + 6, bq); }
        } else {
            s[e] = os; a[e] = oa; q[e] = oq;
        }
    }
    out_cost = best;
    if (best > model.cap) return {};
    return model.program(bs, ba, bq);
}

// ---------------------------------------------------------------- cykl k=2, przeglad wyczerpujacy
// Ticki naprzemiennie A, B. Po odbiorze w B (P wyczyszczone, H czyste):
// odbior w kolejnym B <=> P ⊆ b2 ∪ (a2 \ b3), H ∩ b2 = ∅, H ∩ a2 ⊆ b3.
// Kazdy emiter: stan w A (pozycja, aktywny) + op przejscia A->B (powrot to op odwrotny).
// Koszt emitera = ticki prefiksu potrzebne do ustawienia stanu A; szukamy min max kosztu.
struct C2Opt { int posA, actA, posB, actB; char op; int cost; };

static char reverse_op(char op) { return op == 'F' ? 'B' : op == 'B' ? 'F' : op; }

template <int NW>
static Program cycle2_search(const Case& cs, int maxcost) {
    struct AC { u64 oA[NW], tA[NW], oB[NW], tB[NW]; int i, j, cost; };
    u64 P[NW], H[NW];
    for (int w = 0; w < NW; w++) { P[w] = cs.P.w[w]; H[w] = cs.H.w[w]; }
    for (int c = 1; c <= maxcost; c++) {
        vector<C2Opt> opts[6];
        for (int e = 0; e < 6; e++) {
            opts[e].push_back({cs.home(e), 0, cs.home(e), 0, '-', 0});  // wylaczony
            for (int p = 0; p < cs.L; p++) {
                int d = abs(cs.delta(cs.home(e), p));
                if (!cs.useful[e][p] || d > c) continue;
                // nieaktywny w A, wlaczany na B
                opts[e].push_back({p, 0, p, 1, 'T', d});
                if (d + 1 > c) continue;
                opts[e].push_back({p, 1, p, 1, '-', d + 1});
                opts[e].push_back({p, 1, p, 0, 'T', d + 1});
                int f = (p + 1) % cs.L, b = (p + cs.L - 1) % cs.L;
                if (cs.useful[e][f]) opts[e].push_back({p, 1, f, 1, 'F', d + 1});
                if (cs.useful[e][b]) opts[e].push_back({p, 1, b, 1, 'B', d + 1});
            }
        }
        auto mask = [&](int e, int pos, int act, int w) -> u64 { return act ? cs.ray[e][pos].w[w] : 0; };
        vector<AC> ax[3];
        for (int a = 0; a < 3; a++)
            for (int i = 0; i < (int)opts[a].size(); i++)
                for (int j = 0; j < (int)opts[a + 3].size(); j++) {
                    const C2Opt& x = opts[a][i]; const C2Opt& y = opts[a + 3][j];
                    AC r;
                    r.i = i; r.j = j; r.cost = max(x.cost, y.cost);
                    for (int w = 0; w < NW; w++) {
                        u64 xa = mask(a, x.posA, x.actA, w), ya = mask(a + 3, y.posA, y.actA, w);
                        u64 xb = mask(a, x.posB, x.actB, w), yb = mask(a + 3, y.posB, y.actB, w);
                        r.oA[w] = xa ^ ya; r.tA[w] = xa & ya; r.oB[w] = xb ^ yb; r.tB[w] = xb & yb;
                    }
                    ax[a].push_back(r);
                }
        for (auto& u : ax[0]) {
            for (auto& v : ax[1]) {
                u64 s1A[NW], s2A[NW], g3A[NW], anA[NW], s1B[NW], s2B[NW], g3B[NW], anB[NW];
                bool dead = false;
                for (int w = 0; w < NW; w++) {
                    u64 u0 = u.oA[w] | u.tA[w], u1 = v.oA[w] | v.tA[w];
                    s1A[w] = (u.oA[w] ^ v.oA[w]) & ~(u.tA[w] | v.tA[w]);
                    s2A[w] = (u.tA[w] & ~u1) | (v.tA[w] & ~u0) | (u.oA[w] & v.oA[w]);
                    g3A[w] = (u.tA[w] & u1) | (v.tA[w] & u0);
                    anA[w] = s1A[w] | s2A[w] | g3A[w];
                    u0 = u.oB[w] | u.tB[w]; u1 = v.oB[w] | v.tB[w];
                    s1B[w] = (u.oB[w] ^ v.oB[w]) & ~(u.tB[w] | v.tB[w]);
                    s2B[w] = (u.tB[w] & ~u1) | (v.tB[w] & ~u0) | (u.oB[w] & v.oB[w]);
                    g3B[w] = (u.tB[w] & u1) | (v.tB[w] & u0);
                    anB[w] = s1B[w] | s2B[w] | g3B[w];
                    // komorka P zniszczona w obu tickach juz przez osie 0 i 1
                    if (P[w] & g3A[w] & g3B[w]) dead = true;
                }
                if (dead) continue;
                for (auto& x : ax[2]) {
                    bool ok = true;
                    for (int w = 0; w < NW && ok; w++) {
                        u64 uA = x.oA[w] | x.tA[w], uB = x.oB[w] | x.tB[w];
                        u64 a2 = (s2A[w] & ~uA) | (s1A[w] & x.oA[w]) | (x.tA[w] & ~anA[w]);
                        u64 b2 = (s2B[w] & ~uB) | (s1B[w] & x.oB[w]) | (x.tB[w] & ~anB[w]);
                        u64 b3 = g3B[w] | (s2B[w] & uB) | (s1B[w] & x.tB[w]);
                        if ((P[w] & ~(b2 | (a2 & ~b3))) || (H[w] & b2) || (H[w] & a2 & ~b3)) ok = false;
                    }
                    if (!ok) continue;
                    // znalezione przy koszcie <= c (mniejsze c juz sprawdzone) -> budujemy program
                    const C2Opt* ch[6] = {&opts[0][u.i], &opts[1][v.i], &opts[2][x.i],
                                          &opts[3][u.j], &opts[4][v.j], &opts[5][x.j]};
                    int T0 = 0;
                    for (int e = 0; e < 6; e++) T0 = max(T0, ch[e]->cost);
                    Program prog;
                    for (int t = 1; t <= T0; t++) {
                        string tick(6, '-');
                        for (int e = 0; e < 6; e++) {
                            if (ch[e]->cost == 0) continue;
                            int d = cs.delta(cs.home(e), ch[e]->posA);
                            int st = T0 - abs(d) - ch[e]->actA + 1;
                            if (st <= t && t < st + abs(d)) tick[e] = d > 0 ? 'F' : 'B';
                            else if (ch[e]->actA && t == T0) tick[e] = 'T';
                        }
                        prog.push_back(tick);
                    }
                    string t1(6, '-'), t2(6, '-');
                    for (int e = 0; e < 6; e++) { t1[e] = ch[e]->op; t2[e] = reverse_op(ch[e]->op); }
                    while ((int)prog.size() < T0 + 2 * cs.req + 2) { prog.push_back(t1); prog.push_back(t2); }
                    if (cs.finish(prog)) return prog;
                }
            }
        }
    }
    return {};
}

static Program cycle2(const Case& cs, int maxcost) {
    int cells = 3 * cs.n * cs.n - 3 * cs.n + 1;
    int nw = (cells + 63) / 64;
    switch (nw) {
        case 1: return cycle2_search<1>(cs, maxcost);
        case 2: return cycle2_search<2>(cs, maxcost);
        case 3: return cycle2_search<3>(cs, maxcost);
        case 4: return cycle2_search<4>(cs, maxcost);
        case 5: return cycle2_search<5>(cs, maxcost);
        case 6: return cycle2_search<6>(cs, maxcost);
        default: return cycle2_search<7>(cs, maxcost);
    }
}

// ---------------------------------------------------------------- polerowanie (T5)
static Program greedy_delete(const Case& cs, Program prog) {
    size_t t = 0;
    while (t < prog.size()) {
        Program cand = prog;
        cand.erase(cand.begin() + t);
        if (cs.finish(cand)) prog = cand; else t++;
    }
    return prog;
}

static Program polish(const Case& cs, Program prog, int iterations, Rng& rng) {
    if (!cs.finish(prog)) return {};
    size_t start = prog.size();
    Program best = greedy_delete(cs, prog), cur = best;
    for (int it = 0; it < iterations; it++) {
        int n = cur.size();
        if (n == 0) break;
        int e = rng.below(6), t = rng.below(n);
        double r = rng.uni();
        Program cand = cur;
        if (r < 0.3) {
            for (int i = t; i < n - 1; i++) cand[i][e] = cur[i + 1][e];
            cand[n - 1][e] = '-';
        } else if (r < 0.5) {
            for (int i = n - 1; i > t; i--) cand[i][e] = cur[i - 1][e];
            cand[t][e] = '-';
        } else if (r < 0.75 && t + 1 < n) {
            swap(cand[t][e], cand[t + 1][e]);
        } else {
            cand[t][e] = "FBT-"[rng.below(4)];
        }
        if (!cs.finish(cand) || cand.size() > cur.size()) continue;
        cur = cand;
        if (rng.uni() < 0.2) cur = greedy_delete(cs, cur);
        if (cur.size() < best.size()) best = cur;
    }
    return best.size() < start ? best : Program{};
}

// ---------------------------------------------------------------- main
static vector<string> split(const string& s, char sep) {
    vector<string> out;
    string cur;
    for (char c : s) { if (c == sep) { out.push_back(cur); cur.clear(); } else cur += c; }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "uzycie: level6_opt beam|cycles|polish klucz=wartosc...\n"); return 1; }
    string mode = argv[1];
    map<string, string> opt = {{"names", "0-example,1-small,2-medium,3-large,4-enormous"},
                               {"width", "100"}, {"settings", "3:0.3:2,8:0.5:4,12:1:3,20:0.8:8,5:0.15:3,2:1:2"},
                               {"ks", "1,2,3,4,5,6,8"}, {"iterations", "20000"}, {"restarts", "3"},
                               {"minreq", "2"}, {"seed", "1"}, {"tag", ""}, {"cases", ""},
                               {"slack", "0"}, {"noise", "1"}, {"step", "2"}, {"maxcost", "99"},
                               {"threads", to_string(max(1u, thread::hardware_concurrency()))}};
    for (int i = 2; i < argc; i++) {
        string a = argv[i];
        size_t eq = a.find('=');
        if (eq == string::npos || !opt.count(a.substr(0, eq))) { fprintf(stderr, "nieznana opcja %s\n", a.c_str()); return 1; }
        opt[a.substr(0, eq)] = a.substr(eq + 1);
    }
    int threads = stoi(opt["threads"]), width = stoi(opt["width"]), iterations = stoi(opt["iterations"]);
    int restarts = stoi(opt["restarts"]), minreq = stoi(opt["minreq"]);
    u64 seed = stoull(opt["seed"]);
    g_slack = stoi(opt["slack"]);
    g_noise = stod(opt["noise"]);
    int step = stoi(opt["step"]);
    vector<Settings> settings;
    for (auto& s : split(opt["settings"], ',')) {
        auto p = split(s, ':');
        settings.push_back({stod(p[0]), stod(p[1]), stod(p[2])});
    }
    vector<int> ks;
    for (auto& s : split(opt["ks"], ',')) ks.push_back(stoi(s));
    int lo = 0, hi = INT_MAX;
    if (!opt["cases"].empty()) { auto p = split(opt["cases"], '-'); lo = stoi(p[0]); hi = stoi(p[1]); }

    long total_old = 0, total_new = 0;
    for (auto& name : split(opt["names"], ',')) {
        auto cases = load_cases("../level-6/in_level-6_" + name + ".txt");
        auto base = read_programs("outputs/out_level-6_" + name + ".txt", cases.size());
        vector<Program> result(cases.size());
        atomic<int> next(0);
        auto worker = [&]() {
            while (true) {
                int i = next++;
                if (i >= (int)cases.size()) break;
                if (i < lo || i > hi) continue;
                const Case& cs = cases[i];
                int base_len = base[i].empty() ? 1000000 : base[i].size();
                Rng rng(seed * 1000003 + i);
                Program best;
                int limit = base_len - 1;
                if (mode == "beam") {
                    for (size_t s = 0; s < settings.size(); s++) {
                        Program p = beam(cs, width, settings[s], limit, seed * 7919 + i * 31 + s);
                        if (!p.empty() && (int)p.size() <= limit) { best = p; limit = p.size() - 1; }
                    }
                } else if (mode == "lns") {
                    // Przebudowa sufiksu: tniemy najlepszy program w ticku t i od tego
                    // stanu szukamy krotszej koncowki beamem; po poprawie od nowa.
                    if (base[i].empty()) continue;
                    Program cur = base[i];
                    bool improved = true;
                    int round = 0;
                    while (improved) {
                        improved = false;
                        int len = cur.size();
                        for (int t = len - 2; t >= 0 && !improved; t -= step) {
                            Program prefix(cur.begin(), cur.begin() + t);
                            Node st = state_after(cs, prefix);
                            if (st.acc >= cs.req) continue;
                            for (size_t s = 0; s < settings.size() && !improved; s++) {
                                Program p = beam(cs, width, settings[s], len - t - 1,
                                                 seed * 7919 + i * 31 + s + 101 * (t + 1) + 7777 * round, &st, prefix);
                                if (p.empty()) continue;
                                Program cand = prefix;
                                cand.insert(cand.end(), p.begin(), p.end());
                                if (cs.finish(cand) && cand.size() < cur.size()) { cur = cand; improved = true; }
                            }
                        }
                        round++;
                    }
                    if (cur.size() < base[i].size()) best = cur;
                } else if (mode == "cycles") {
                    if (cs.req < minreq) continue;
                    for (int k : ks)
                        for (int r = 0; r < restarts; r++) {
                            CycleModel model(cs, k, limit);
                            int cost;
                            Program p = anneal_cycles(model, rng, iterations, cost);
                            if (!p.empty() && (int)p.size() <= limit) { best = p; limit = p.size() - 1; }
                        }
                } else if (mode == "cycle2") {
                    int maxcost = min(stoi(opt["maxcost"]), limit - (2 * cs.req - 1));
                    if (cs.req >= minreq && maxcost >= 1) {
                        Program p = cycle2(cs, maxcost);
                        if (!p.empty() && (int)p.size() <= limit) best = p;
                    }
                } else if (mode == "polish") {
                    if (!base[i].empty()) best = polish(cs, base[i], iterations, rng);
                }
                result[i] = best;
            }
        };
        vector<thread> pool;
        for (int t = 0; t < threads; t++) pool.emplace_back(worker);
        for (auto& t : pool) t.join();
        long old = 0, now = 0;
        int improved = 0;
        for (size_t i = 0; i < cases.size(); i++) {
            old += base[i].size();
            if (!result[i].empty()) { improved++; now += result[i].size(); } else now += base[i].size();
        }
        total_old += old; total_new += now;
        printf("%s: %ld -> %ld (poprawione %d/%zu)\n", name.c_str(), old, now, improved, cases.size());
        fflush(stdout);
        write_programs("candidates/" + mode + opt["tag"] + "/out_level-6_" + name + ".txt", result);
    }
    printf("TOTAL: %ld -> %ld\n", total_old, total_new);
}
