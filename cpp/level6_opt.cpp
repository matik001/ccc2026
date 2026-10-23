// Level 6: C++20 optimizer, based on the colleague's solver at 7179050.
// Run from the repository root; see cpp/README.md for commands and results.
// Default baseline: python/outputs. Candidates: cpp/candidates/<mode><tag>.
// The merge mode validates and selects the shortest program for each case.

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
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
static inline int pc(const B& a) { int s = 0; for (int i = 0; i < WD; i++) s += std::popcount(a.w[i]); return s; }
static inline bool bnone(const B& a) { u64 s = 0; for (int i = 0; i < WD; i++) s |= a.w[i]; return s == 0; }
static inline bool beq(const B& a, const B& b) { return memcmp(a.w, b.w, sizeof a.w) == 0; }
static inline void bset(B& a, int i) { a.w[i >> 6] |= 1ULL << (i & 63); }

// Search uses exactly as many machine words as this board needs. Medium
// boards fit in one word; using seven here wasted most of the inner-loop work.
template<int N> struct SmallB { u64 w[N]{}; };
#define SMALL_BOP(name, expr) \
    template<int N> static inline SmallB<N> name(const SmallB<N>& a, const SmallB<N>& b) { \
        SmallB<N> r; for (int i = 0; i < N; ++i) r.w[i] = (expr); return r; }
SMALL_BOP(bor, a.w[i] | b.w[i])
SMALL_BOP(band, a.w[i] & b.w[i])
SMALL_BOP(bxor, a.w[i] ^ b.w[i])
SMALL_BOP(bandn, a.w[i] & ~b.w[i])
template<int N> static inline int pc(const SmallB<N>& a) {
    int result = 0; for (auto word : a.w) result += std::popcount(word); return result;
}
template<int N> static inline bool bnone(const SmallB<N>& a) {
    u64 value = 0; for (auto word : a.w) value |= word; return value == 0;
}
template<int N> static inline bool beq(const SmallB<N>& a, const SmallB<N>& b) {
    return memcmp(a.w, b.w, sizeof a.w) == 0;
}
template<int N> static inline SmallB<N> smallBits(const B& a) {
    SmallB<N> out; for (int i = 0; i < N; ++i) out.w[i] = a.w[i]; return out;
}

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
static bool g_history = false; // optionally suppress states retained at earlier depths
static double g_capture = 1e6; // 0: value a capture as the full target's matter
static bool g_adaptive = false;
static int g_optical_quota = 0, g_matter_quota = 0;
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
        if (tick.size() != 6 || tick.find_first_not_of("FBT-") != string::npos) {
            fprintf(stderr, "niepoprawna instrukcja w %s\n", path.c_str());
            return vector<Program>(count);
        }
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

template<typename SearchState> static u64 node_hash(const SearchState& x, const Case& cs, bool active_only = false) {
    u64 h = 1469598103934665603ULL;
    auto mix = [&](u64 v) { h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2); h *= 1099511628211ULL; };
    // Opposing emitters are optically interchangeable. Keep the representative's
    // real positions and commands; only deduplication uses the canonical pair.
    for (int axis = 0; axis < 3; ++axis) {
        int a = cs.delta(cs.home(axis), x.pos[axis]);
        int b = -cs.delta(cs.home(axis + 3), x.pos[axis + 3]);
        int aa = (x.act >> axis) & 1, ba = (x.act >> (axis + 3)) & 1;
        if (active_only) { if (!aa) a = 0; if (!ba) b = 0; }
        pair<int, int> first{a, aa}, second{b, ba};
        if (second < first) swap(first, second);
        mix(static_cast<u64>(first.first + cs.L)); mix(first.second);
        mix(static_cast<u64>(second.first + cs.L)); mix(second.second);
    }
    mix(x.acc);
    for (u64 word : x.m.w) mix(word);
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
template<int NW>
static Program beam_impl(const Case& cs, int width, Settings st, int limit, u64 seed, const Node* from = nullptr,
                    const Program& prefix = {}) {
    using BB = SmallB<NW>;
    struct Node {
        int pos[6]; int act, acc; BB m; int parent; char ops[7]; double score;
    };
    struct Opt { char op; int pos; int act; BB mask; };
    const BB P = smallBits<NW>(cs.P), H = smallBits<NW>(cs.H);
    vector<BB> rays[6];
    for (int e = 0; e < 6; ++e) {
        rays[e].reserve(cs.L);
        for (const auto& mask : cs.ray[e]) rays[e].push_back(smallBits<NW>(mask));
    }
    Rng rng(seed);
    const BB relevant = bor(P, H);
    vector<vector<Node>> layers;
    Node start{};
    if (from) {
        copy(from->pos, from->pos + 6, start.pos);
        start.act = from->act; start.acc = from->acc;
        start.m = band(smallBits<NW>(from->m), relevant);
    } else {
        for (int e = 0; e < 6; ++e) start.pos[e] = cs.home(e);
    }
    strcpy(start.ops, "------");
    start.parent = -1;
    layers.push_back({start});
    unordered_set<u64> history;
    if (g_history) history.insert(node_hash(start, cs));
    struct Child { double score; int parent, i01, i2; BB m; int acc; };
    struct AxisC { int i, j; BB one, two; double pot; };
    struct PairC { int a, b; BB s1, s2, g3, any; double pot; };
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
            BB missing = bandn(P, nd.m), dirty = band(H, nd.m);
            auto& opts = all_opts[pi];
            double pots[6][4];
            for (int e = 0; e < 6; e++) {
                int p = nd.pos[e], a = nd.act >> e & 1;
                auto add = [&](char op, int np, int na) {
                    const BB& r = rays[e][np];
                    pots[e][opts[e].size()] = pc(band(r, missing)) + st.beta * pc(band(r, dirty));
                    opts[e].push_back({op, np, na, na ? r : BB{}});
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
                        const BB& x = opts[a][i].mask; const BB& y = opts[a + 3][j].mask;
                        ax[a][axn[a]++] = {i, j, bxor(x, y), band(x, y), pots[a][i] + pots[a + 3][j]};
                    }
            }
            auto& pairs = all_pairs[pi];
            for (int u = 0; u < axn[0]; u++)
                for (int v = 0; v < axn[1]; v++) {
                    const AxisC& A = ax[0][u]; const AxisC& C = ax[1][v];
                    BB u0 = bor(A.one, A.two), u1 = bor(C.one, C.two);
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
                    BB u2 = bor(T.one, T.two);
                    BB exact2 = bor(bor(bandn(Q.s2, u2), band(Q.s1, T.one)), bandn(T.two, Q.any));
                    BB ge3 = bor(bor(Q.g3, band(Q.s2, u2)), band(Q.s1, T.two));
                    // Distant matter neither obstructs beams nor affects a
                    // submission. Discard it to merge equivalent search states.
                    ch.m = band(bandn(bor(nd.m, exact2), ge3), relevant);
                    ch.acc = nd.acc;
                    if (beq(band(ch.m, P), P) && bnone(band(ch.m, H))) {
                        ch.acc++;
                        ch.m = bandn(ch.m, P);
                        if (ch.acc == cs.req) { done_parent = pi; done01 = u; done2 = v; break; }
                    }
                    int filled = pc(band(ch.m, P));
                    double alpha = st.alpha;
                    if (g_adaptive) {
                        double fraction = static_cast<double>(filled) / cs.pcount;
                        alpha *= 0.2 + 0.8 * fraction * fraction;
                    }
                    ch.score = ch.acc * (g_capture == 0 ? 4.0 * cs.pcount : g_capture) + 4.0 * filled - 4.0 * alpha * pc(band(ch.m, H)) +
                               st.gamma * (Q.pot + T.pot) + g_noise * rng.uni();
                    ch.parent = pi; ch.i01 = u; ch.i2 = v;
                    local.push_back(ch);
                }
                if (done_parent >= 0) break;
            }
            if (done_parent >= 0) break;
            int local_limit = (g_optical_quota || g_matter_quota) ? 2 * width : width;
            if ((int)local.size() > local_limit) {
                nth_element(local.begin(), local.begin() + local_limit, local.end(),
                            [](const Child& x, const Child& y) { return x.score > y.score; });
                local.resize(local_limit);
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
        unordered_map<u64, int> optical_groups, matter_groups;
        for (auto& ch : pool) {
            if ((int)next.size() >= width) break;
            Node nd;
            materialize(ch.parent, ch.i01, ch.i2, nd);
            nd.m = ch.m; nd.acc = ch.acc; nd.score = ch.score;
            u64 hash = node_hash(nd, cs);
            if ((g_history && history.count(hash)) || seen.count(hash)) continue;
            u64 optical = 0, matter = nd.acc + 1469598103934665603ULL;
            if (g_optical_quota) {
                optical = node_hash(nd, cs, true);
                if (optical_groups[optical] >= g_optical_quota) continue;
            }
            if (g_matter_quota) {
                for (auto word : nd.m.w) { matter ^= word; matter *= 1099511628211ULL; }
                if (matter_groups[matter] >= g_matter_quota) continue;
            }
            seen.insert(hash);
            if (g_optical_quota) ++optical_groups[optical];
            if (g_matter_quota) ++matter_groups[matter];
            next.push_back(nd);
        }
        if (next.empty()) return {};
        if (g_history) history.insert(seen.begin(), seen.end());
        layers.push_back(move(next));
    }
    return {};
}

static Program beam(const Case& cs, int width, Settings st, int limit, u64 seed,
                    const Node* from = nullptr, const Program& prefix = {}) {
    int words = (3 * cs.n * (cs.n - 1) + 1 + 63) / 64;
    switch (words) {
        case 1: return beam_impl<1>(cs, width, st, limit, seed, from, prefix);
        case 2: return beam_impl<2>(cs, width, st, limit, seed, from, prefix);
        case 3: return beam_impl<3>(cs, width, st, limit, seed, from, prefix);
        case 4: return beam_impl<4>(cs, width, st, limit, seed, from, prefix);
        case 5: return beam_impl<5>(cs, width, st, limit, seed, from, prefix);
        case 6: return beam_impl<6>(cs, width, st, limit, seed, from, prefix);
        default: return beam_impl<7>(cs, width, st, limit, seed, from, prefix);
    }
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

#include "level6_cycles.hpp"

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

// Delete a tick, then repair the shortened schedule. Unlike polish, this can
// cross temporarily invalid programs. Prefix caching only replays changed tails.
template<int NW>
static Program repair_impl(const Case& cs, Program best, int iterations, int restarts, Rng& rng) {
    using BB = SmallB<NW>;
    struct State {
        array<int, 6> pos{};
        int active = 0, captures = 0, best_deficit = 0, last_deficit = 0;
        int capture_times = 0;
        BB matter{};
    };
    const int original_size = static_cast<int>(best.size());
    const BB target = smallBits<NW>(cs.P), halo = smallBits<NW>(cs.H);
    const BB relevant = bor(target, halo);
    vector<BB> rays[6];
    for (int e = 0; e < 6; ++e)
        for (const auto& r : cs.ray[e]) rays[e].push_back(smallBits<NW>(r));
    State initial;
    initial.best_deficit = initial.last_deficit = cs.pcount;
    for (int e = 0; e < 6; ++e) initial.pos[e] = cs.home(e);
    auto replay = [&](const Program& program, vector<State>& cache, int first) {
        if (first == 0) cache[0] = initial;
        for (int t = first; t < static_cast<int>(program.size()); ++t) {
            State st = cache[t];
            BB one{}, two{}, three{};
            for (int e = 0; e < 6; ++e) {
                char op = program[t][e];
                if (op == 'F') { if (++st.pos[e] == cs.L) st.pos[e] = 0; }
                else if (op == 'B') { if (--st.pos[e] < 0) st.pos[e] = cs.L - 1; }
                else if (op == 'T') st.active ^= 1 << e;
                if (!(st.active & (1 << e))) continue;
                const BB& ray = rays[e][st.pos[e]];
                for (int w = 0; w < NW; ++w) {
                    const u64 o = one.w[w], d = two.w[w], h = three.w[w], x = ray.w[w];
                    three.w[w] = h | (d & x);
                    two.w[w] = (d & ~x) | (o & x);
                    one.w[w] = (o & ~x) | (x & ~(o | d | h));
                }
            }
            st.matter = band(bandn(bor(st.matter, two), three), relevant);
            st.last_deficit = pc(bandn(target, st.matter)) + 2 * pc(band(halo, st.matter));
            if (st.last_deficit == 0) {
                ++st.captures;
                st.capture_times += t + 1;
                st.matter = bandn(st.matter, target);
                st.best_deficit = st.last_deficit = cs.pcount;
                cache[t + 1] = st;
                if (st.captures == cs.req) return t + 1;
            } else {
                st.best_deficit = min(st.best_deficit, st.last_deficit);
                cache[t + 1] = st;
            }
        }
        return -1;
    };
    auto energy = [&](const State& st) {
        return (cs.req - st.captures) * (10.0 + 4 * (cs.pcount + 2 * pc(halo))) +
               4.0 * st.best_deficit + 0.05 * st.last_deficit + 0.0001 * st.capture_times;
    };
    const int batch = max(1, iterations / restarts);
    int spent = 0;
    while (spent < iterations && best.size() > 1) {
        int n = static_cast<int>(best.size()) - 1;
        int cut = rng.below(n + 1);
        Program cur(n, "------");
        for (int e = 0; e < 6; ++e) {
            int remove = cut;
            if (rng.uni() < 0.75) {
                vector<int> idle;
                for (int t = max(0, cut - 5); t <= min(n, cut + 5); ++t)
                    if (best[t][e] == '-') idle.push_back(t);
                if (!idle.empty()) remove = idle[rng.below(static_cast<int>(idle.size()))];
            }
            for (int t = 0; t < n; ++t) cur[t][e] = best[t + (t >= remove)][e];
        }
        vector<State> cache(n + 1), trial(n + 1);
        int solved = replay(cur, cache, 0);
        if (solved >= 0) {
            cur.resize(solved);
            if (!cs.finish(cur)) return {};
            best = move(cur);
            ++spent;
            continue;
        }
        double value = energy(cache.back());
        for (int it = 0; it < batch && spent < iterations; ++it, ++spent) {
            Program candidate = cur;
            int e = rng.below(6);
            int t = rng.uni() < 0.55 ? clamp(cut + rng.below(17) - 8, 0, n - 1) : rng.below(n);
            int first = t;
            double move_type = rng.uni();
            if (move_type < 0.55) {
                int other = clamp(t + rng.below(9) - 4, 0, n - 1);
                swap(candidate[t][e], candidate[other][e]);
                first = min(t, other);
            } else if (move_type < 0.85) {
                candidate[t][e] = "FBT-"[rng.below(4)];
            } else {
                // Relocate one emitter's operation, preserving its total motion.
                int other = rng.below(n);
                char op = candidate[t][e];
                if (other < t) for (int j = t; j > other; --j) candidate[j][e] = candidate[j - 1][e];
                else for (int j = t; j < other; ++j) candidate[j][e] = candidate[j + 1][e];
                candidate[other][e] = op;
                first = min(t, other);
            }
            if (candidate == cur) continue;
            trial[first] = cache[first];
            solved = replay(candidate, trial, first);
            if (solved >= 0) {
                candidate.resize(solved);
                if (!cs.finish(candidate)) return {};
                best = move(candidate);
                ++spent;
                break;
            }
            double next = energy(trial.back());
            double temp = 3.0 * (1.0 - static_cast<double>(it) / batch) + 0.08;
            if (next <= value || rng.uni() < exp((value - next) / temp)) {
                cur = move(candidate);
                copy(trial.begin() + first + 1, trial.end(), cache.begin() + first + 1);
                value = next;
            }
        }
    }
    return static_cast<int>(best.size()) < original_size ? best : Program{};
}

static Program repair(const Case& cs, const Program& prog, int iterations, int restarts, Rng& rng) {
    int words = (3 * cs.n * (cs.n - 1) + 64) / 64;
    switch (words) {
        case 1: return repair_impl<1>(cs, prog, iterations, restarts, rng);
        case 2: return repair_impl<2>(cs, prog, iterations, restarts, rng);
        case 3: return repair_impl<3>(cs, prog, iterations, restarts, rng);
        case 4: return repair_impl<4>(cs, prog, iterations, restarts, rng);
        case 5: return repair_impl<5>(cs, prog, iterations, restarts, rng);
        case 6: return repair_impl<6>(cs, prog, iterations, restarts, rng);
        default: return repair_impl<7>(cs, prog, iterations, restarts, rng);
    }
}

// Mine closed emitter schedules from a known solution. A later, efficient
// cycle may be reachable directly without the solution's exploratory prefix.
static Program recycle(const Case& cs, const Program& source) {
    struct Emitters { array<int, 6> pos{}; int active = 0; };
    vector<Emitters> states(source.size() + 1);
    for (int e = 0; e < 6; ++e) states[0].pos[e] = cs.home(e);
    for (int t = 0; t < static_cast<int>(source.size()); ++t) {
        states[t + 1] = states[t];
        auto& st = states[t + 1];
        for (int e = 0; e < 6; ++e) {
            char op = source[t][e];
            if (op == 'F') st.pos[e] = (st.pos[e] + 1) % cs.L;
            else if (op == 'B') st.pos[e] = (st.pos[e] + cs.L - 1) % cs.L;
            else if (op == 'T') st.active ^= 1 << e;
        }
    }
    Program best = source;
    for (int start = 0; start < static_cast<int>(source.size()); ++start) {
        const auto& a = states[start];
        for (int period = 1; period <= 24 && start + period <= static_cast<int>(source.size()); ++period) {
            const auto& b = states[start + period];
            int swaps = 0;
            bool closed = true;
            for (int axis = 0; axis < 3; ++axis) {
                auto coordinate = [&](const Emitters& st, int e) {
                    int offset = cs.delta(cs.home(e), st.pos[e]);
                    return pair<int, int>{e < 3 ? offset : -offset, (st.active >> e) & 1};
                };
                auto a1 = coordinate(a, axis), a2 = coordinate(a, axis + 3);
                auto b1 = coordinate(b, axis), b2 = coordinate(b, axis + 3);
                if (a1 == b1 && a2 == b2) continue;
                if (a1 == b2 && a2 == b1) swaps |= 1 << axis;
                else { closed = false; break; }
            }
            if (!closed) continue;
            int setup = 0;
            for (int e = 0; e < 6; ++e)
                setup = max(setup, abs(cs.delta(cs.home(e), a.pos[e])) + ((a.active >> e) & 1));
            for (int original_prefix = 0; original_prefix < 2; ++original_prefix) {
                Program candidate;
                if (original_prefix) candidate.assign(source.begin(), source.begin() + start);
                else {
                    for (int t = 1; t <= setup; ++t) {
                        string tick(6, '-');
                        for (int e = 0; e < 6; ++e) {
                            int d = cs.delta(cs.home(e), a.pos[e]), active = (a.active >> e) & 1;
                            int first = setup - abs(d) - active + 1;
                            if (t >= first && t < first + abs(d)) tick[e] = d > 0 ? 'F' : 'B';
                            else if (active && t == setup) tick[e] = 'T';
                        }
                        candidate.push_back(tick);
                    }
                }
                if (candidate.size() >= best.size()) continue;
                for (int repetition = 0; candidate.size() + 1 < best.size(); ++repetition) {
                    for (int phase = 0; phase < period && candidate.size() + 1 < best.size(); ++phase) {
                        string tick = source[start + phase];
                        if (repetition % 2) {
                            for (int axis = 0; axis < 3; ++axis) if (swaps & (1 << axis)) {
                                char x = tick[axis], y = tick[axis + 3];
                                tick[axis] = reverse_op(y);
                                tick[axis + 3] = reverse_op(x);
                            }
                        }
                        candidate.push_back(tick);
                    }
                }
                if (cs.finish(candidate) && candidate.size() < best.size()) best = move(candidate);
            }
        }
    }
    return best.size() < source.size() ? best : Program{};
}

#include "level6_reverse.hpp"

// ---------------------------------------------------------------- main
static vector<string> split(const string& s, char sep) {
    vector<string> out;
    string cur;
    for (char c : s) { if (c == sep) { out.push_back(cur); cur.clear(); } else cur += c; }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "uzycie: level6_opt beam|lns|cycles|cycle2|cyclesa|cyclesym|reverse|polish|repair|recycle|merge|verify klucz=wartosc...\n"); return 1; }
    string mode = argv[1];
    map<string, string> opt = {{"names", "0-example,1-small,2-medium,3-large,4-enormous"},
                               {"width", "100"}, {"settings", "3:0.3:2,8:0.5:4,12:1:3,20:0.8:8,5:0.15:3,2:1:2"},
                               {"ks", "1,2,3,4,5,6,8"}, {"iterations", "20000"}, {"restarts", "3"},
                               {"minreq", "2"}, {"seed", "1"}, {"tag", ""}, {"cases", ""},
                               {"threads", to_string(min(4u, max(1u, thread::hardware_concurrency())))},
                               {"inputdir", "level-6"}, {"baseline", "python/outputs"},
                               {"outdir", "cpp/candidates"}, {"sources", ""},
                               {"slack", "0"}, {"noise", "1"}, {"step", "2"}, {"maxcost", "99"},
                               {"progress", "1"}, {"history", "0"}, {"segs", ""}, {"capture", "1000000"}, {"adaptive", "0"},
                               {"optical", "0"}, {"matter", "0"}};
    for (int i = 2; i < argc; i++) {
        string a = argv[i];
        size_t eq = a.find('=');
        if (eq == string::npos || !opt.count(a.substr(0, eq))) { fprintf(stderr, "nieznana opcja %s\n", a.c_str()); return 1; }
        opt[a.substr(0, eq)] = a.substr(eq + 1);
    }
    int threads = stoi(opt["threads"]), width = stoi(opt["width"]), iterations = stoi(opt["iterations"]);
    int restarts = stoi(opt["restarts"]), minreq = stoi(opt["minreq"]);
    if (threads < 1 || width < 1 || iterations < 0 || restarts < 1) {
        fprintf(stderr, "threads/width/restarts musza byc dodatnie; iterations >= 0\n");
        return 1;
    }
    if (mode != "beam" && mode != "cycles" && mode != "cyclesa" && mode != "cyclesym" && mode != "polish" && mode != "repair" && mode != "recycle" && mode != "reverse" && mode != "merge" && mode != "verify" && mode != "lns" && mode != "cycle2") {
        fprintf(stderr, "nieznany tryb %s\n", mode.c_str());
        return 1;
    }
    u64 seed = stoull(opt["seed"]);
    g_slack = stoi(opt["slack"]);
    g_noise = stod(opt["noise"]);
    g_history = stoi(opt["history"]) != 0;
    g_capture = stod(opt["capture"]);
    g_adaptive = stoi(opt["adaptive"]) != 0;
    g_optical_quota = stoi(opt["optical"]); g_matter_quota = stoi(opt["matter"]);
    if (g_optical_quota < 0 || g_matter_quota < 0) { fprintf(stderr, "limity roznorodnosci musza byc nieujemne\n"); return 1; }
    int step = stoi(opt["step"]);
    if (step < 1) { fprintf(stderr, "step musi byc dodatni\n"); return 1; }
    vector<Settings> settings;
    for (auto& s : split(opt["settings"], ',')) {
        auto p = split(s, ':');
        if (p.size() != 3) { fprintf(stderr, "settings wymaga alpha:gamma:beta\n"); return 1; }
        settings.push_back({stod(p[0]), stod(p[1]), stod(p[2])});
    }
    vector<int> ks;
    for (auto& s : split(opt["ks"], ',')) ks.push_back(stoi(s));
    vector<vector<int>> segspecs;
    if (opt["segs"].empty()) for (int k : ks) segspecs.push_back({k});
    else for (auto& spec : split(opt["segs"], ',')) {
        vector<int> segments;
        for (auto& value : split(spec, '+')) {
            int length = stoi(value);
            if (length < 1) { fprintf(stderr, "segment musi byc dodatni\n"); return 1; }
            segments.push_back(length);
        }
        segspecs.push_back(move(segments));
    }
    int lo = 0, hi = INT_MAX;
    if (!opt["cases"].empty()) { auto p = split(opt["cases"], '-'); lo = stoi(p[0]); hi = p.size() > 1 ? stoi(p[1]) : lo; }

    long total_old = 0, total_new = 0;
    for (auto& name : split(opt["names"], ',')) {
        auto started = chrono::steady_clock::now();
        auto cases = load_cases(opt["inputdir"] + "/in_level-6_" + name + ".txt");
        string filename = "out_level-6_" + name + ".txt";
        auto base = read_programs(opt["baseline"] + "/" + filename, cases.size());
        for (size_t i = 0; i < cases.size(); ++i) {
            if (base[i].empty() || cases[i].run(base[i]) < 0) {
                fprintf(stderr, "brak poprawnego baseline: %s wzor %zu\n", name.c_str(), i);
                return 2;
            }
        }
        if (mode == "merge" || mode == "verify") {
            auto best = base;
            vector<string> sources = split(opt["sources"], ',');
            if (mode == "merge" && sources.empty() && filesystem::exists(opt["outdir"]))
                for (const auto& entry : filesystem::directory_iterator(opt["outdir"]))
                    if (entry.is_directory()) sources.push_back(entry.path().string());
            sort(sources.begin(), sources.end());
            for (const auto& source : sources) {
                auto proposed = read_programs(source + "/" + filename, cases.size());
                for (size_t i = 0; i < cases.size(); ++i) {
                    if (proposed[i].empty()) continue;
                    if (cases[i].finish(proposed[i]) && proposed[i].size() < best[i].size())
                        best[i] = move(proposed[i]);
                }
            }
            long before = 0, after = 0;
            int wins = 0;
            for (size_t i = 0; i < cases.size(); ++i) {
                before += static_cast<long>(base[i].size());
                after += static_cast<long>(best[i].size());
                wins += best[i].size() < base[i].size();
            }
            if (mode == "merge") write_programs(opt["outdir"] + "/" + filename, best);
            printf("%s: %ld -> %ld (poprawione %d/%zu)\n", name.c_str(), before, after, wins, cases.size());
            total_old += before; total_new += after;
            continue;
        }
        vector<Program> result(cases.size());
        atomic<int> next(0);
        atomic<int> completed(0);
        mutex result_mutex;
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
                    int maxcost = min({stoi(opt["maxcost"]), limit - (2 * cs.req - 1), cs.n + 1});
                    if (cs.req >= minreq && maxcost >= 1) {
                        Program p = cycle2(cs, maxcost);
                        if (!p.empty() && (int)p.size() <= limit) best = p;
                    }
                } else if (mode == "cyclesa" || mode == "cyclesym") {
                    if (cs.req >= minreq) best = closed_cycles(cs, base[i], segspecs, iterations, restarts, rng, mode == "cyclesym");
                } else if (mode == "polish") {
                    if (!base[i].empty()) best = polish(cs, base[i], iterations, rng);
                } else if (mode == "repair") {
                    best = repair(cs, base[i], iterations, restarts, rng);
                } else if (mode == "recycle") {
                    best = recycle(cs, base[i]);
                } else if (mode == "reverse") {
                    best = reverse_polish(cs, base[i], width, rng);
                }
                {
                    lock_guard<mutex> lock(result_mutex);
                    result[i] = best;
                    int done = ++completed;
                    if (stoi(opt["progress"]) && (done % 20 == 0 || !best.empty())) {
                        double seconds = chrono::duration<double>(chrono::steady_clock::now() - started).count();
                        fprintf(stderr, "%s case=%d %d->%zu completed=%d time=%.1fs\n", name.c_str(), i,
                                base_len, best.empty() ? base[i].size() : best.size(), done, seconds);
                        fflush(stderr);
                    }
                    if (!best.empty()) write_programs(opt["outdir"] + "/" + mode + opt["tag"] + "/" + filename, result);
                }
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
        double seconds = chrono::duration<double>(chrono::steady_clock::now() - started).count();
        printf("%s: %ld -> %ld (poprawione %d/%zu; %.3f s)\n", name.c_str(), old, now, improved, cases.size(), seconds);
        fflush(stdout);
        write_programs(opt["outdir"] + "/" + mode + opt["tag"] + "/" + filename, result);
    }
    printf("TOTAL: %ld -> %ld\n", total_old, total_new);
}
