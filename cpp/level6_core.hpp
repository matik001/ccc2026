#pragma once
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

