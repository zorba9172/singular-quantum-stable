// SINGULAR Ω v24 causal_machine — omega-bench: incremental (Ω dirty-cone) vs full recomputation.
//   omega-bench <csv_out>
// Family A: dependency graphs (software implementation of the Ω TRACK/DIRTY/PROP/VALID semantics
//           generalised to 10^6 nodes) on two topologies: partitioned components and a random
//           sliding-window expander (where cones become large and incremental work loses).
// Family C: 1-D explicit heat equation (FTCS). A local change of the initial condition has a
//           light cone of width 2t+1 at step t; only the cone is recomputed (same mechanism,
//           interval representation). Accuracy requirement: bit-identical to full recomputation.
// Every incremental result is compared bit-for-bit against the full recomputation.
// Build: clang++ -std=c++20 -O3 -ffp-contract=off.
#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace {

double now_ns() { return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
long max_rss_mb() { rusage u{}; getrusage(RUSAGE_SELF, &u); return u.ru_maxrss / (1024 * 1024); }  // macOS: bytes

// ---------------- Family A: dependency graph ----------------
struct Graph {
    int n = 0, leaves = 0;
    std::vector<int> pred;     // 4 per node (unused for leaves)
    std::vector<uint8_t> is_leaf;
    std::vector<int> live_leaves;  // leaves with at least one dependent (changes to others are no-ops)
    std::vector<double> w;     // 4 per node
    std::vector<int> succ_off, succ;
};

Graph make_graph(int n, int leaves, int components, int window, uint64_t seed) {
    Graph g; g.n = n;
    const int leaf_stride = n / leaves;  // leaves interleaved through the graph so every region has inputs
    g.pred.assign(4 * size_t(n), -1); g.w.assign(4 * size_t(n), 0.0);
    std::mt19937_64 r(seed);
    std::uniform_real_distribution<double> wd(0.1, 0.3);
    const int comp_size = n / components;
    std::vector<int> deg(n, 0);
    g.is_leaf.assign(n, 0);
    for (int i = 0; i < n; ++i) {
        int lo;
        if (components > 1) lo = (i / comp_size) * comp_size;   // stay within the component
        else lo = std::max(0, i - window);                      // sliding window
        if (i % leaf_stride == 0 || i == lo || (components > 1 && i % comp_size == 0)) { g.is_leaf[i] = 1; ++g.leaves; continue; }
        for (int k = 0; k < 4; ++k) {
            int p = lo + static_cast<int>(r() % static_cast<uint64_t>(std::max(1, i - lo)));
            g.pred[4 * size_t(i) + k] = p; g.w[4 * size_t(i) + k] = wd(r); ++deg[p];
        }
    }
    g.succ_off.assign(n + 1, 0);
    for (int i = 0; i < n; ++i) g.succ_off[i + 1] = g.succ_off[i] + deg[i];
    g.succ.assign(g.succ_off[n], 0);
    std::vector<int> fill(g.succ_off.begin(), g.succ_off.end() - 1);
    for (int i = 0; i < n; ++i) if (g.is_leaf[i] && deg[i] > 0) g.live_leaves.push_back(i);
    for (int i = 0; i < n; ++i) if (!g.is_leaf[i]) for (int k = 0; k < 4; ++k) g.succ[fill[g.pred[4 * size_t(i) + k]]++] = i;
    return g;
}

inline void compute_node(const Graph& g, int i, std::vector<double>& v) {
    const int* p = &g.pred[4 * size_t(i)]; const double* w = &g.w[4 * size_t(i)];
    double s = w[0] * v[p[0]] + w[1] * v[p[1]] + w[2] * v[p[2]] + w[3] * v[p[3]];
    for (int it = 0; it < 8; ++it) s = s - s * s * s * 1e-3;
    v[i] = s;
}

void full_recompute(const Graph& g, std::vector<double>& v) { for (int i = 0; i < g.n; ++i) if (!g.is_leaf[i]) compute_node(g, i, v); }

// Ω semantics in software: DIRTY the changed leaves (version++), PROP marks the transitive
// cone dirty (version++ on newly dirtied), recompute in topological order, CLEAN each node.
struct OmegaState { std::vector<uint32_t> version; std::vector<uint8_t> dirty; std::vector<int> stack, marked; uint64_t epoch = 0; };

size_t incremental(const Graph& g, const std::vector<int>& changed, std::vector<double>& v, OmegaState& st) {
    int lo = g.n, hi = -1;
    st.stack.clear();
    for (int c : changed) { st.version[c]++; st.dirty[c] = 1; st.stack.push_back(c); }
    while (!st.stack.empty()) {
        int x = st.stack.back(); st.stack.pop_back();
        for (int k = g.succ_off[x]; k < g.succ_off[x + 1]; ++k) {
            int y = g.succ[k];
            if (!st.dirty[y]) { st.dirty[y] = 1; st.version[y]++; st.stack.push_back(y); lo = std::min(lo, y); hi = std::max(hi, y); }
        }
    }
    size_t recomputed = 0;
    for (int i = lo; i <= hi; ++i) if (st.dirty[i] && !g.is_leaf[i]) { compute_node(g, i, v); st.dirty[i] = 0; ++recomputed; }
    for (int c : changed) st.dirty[c] = 0;
    return recomputed;
}

// Crossover logic: identical marking, but once the cone exceeds `budget` nodes the incremental
// plan is abandoned (marks undone in O(marked)), the graph is recomputed in full and a global
// epoch is bumped so every outstanding validity certificate (version, epoch) becomes invalid.
size_t adaptive(const Graph& g, const std::vector<int>& changed, std::vector<double>& v, OmegaState& st, size_t budget, bool& fell_back) {
    int lo = g.n, hi = -1;
    st.stack.clear(); st.marked.clear(); fell_back = false;
    for (int c : changed) { st.version[c]++; st.dirty[c] = 1; st.stack.push_back(c); st.marked.push_back(c); }
    while (!st.stack.empty()) {
        int x = st.stack.back(); st.stack.pop_back();
        for (int k = g.succ_off[x]; k < g.succ_off[x + 1]; ++k) {
            int y = g.succ[k];
            if (st.dirty[y]) continue;
            st.dirty[y] = 1; st.version[y]++; st.stack.push_back(y); st.marked.push_back(y);
            lo = std::min(lo, y); hi = std::max(hi, y);
            if (st.marked.size() > budget) {
                for (int m : st.marked) st.dirty[m] = 0;
                full_recompute(g, v);
                ++st.epoch; fell_back = true;
                return static_cast<size_t>(g.n - g.leaves);
            }
        }
    }
    size_t recomputed = 0;
    for (int i = lo; i <= hi; ++i) if (st.dirty[i] && !g.is_leaf[i]) { compute_node(g, i, v); ++recomputed; }
    for (int m : st.marked) st.dirty[m] = 0;
    return recomputed;
}

void family_a(FILE* csv, const char* topo, int components, int window, int& failures) {
    const int n = 1'000'000, leaves = 100'000;
    Graph g = make_graph(n, leaves, components, window, 42);
    std::printf("A %s nodes=%d leaves=%d live_leaves=%zu\n", topo, n, g.leaves, g.live_leaves.size());
    std::mt19937_64 r(7);
    std::uniform_real_distribution<double> d(-1, 1);
    std::vector<double> base(n);
    for (int i = 0; i < n; ++i) if (g.is_leaf[i]) base[i] = d(r);
    full_recompute(g, base);
    OmegaState st{std::vector<uint32_t>(n, 0), std::vector<uint8_t>(n, 0), {}, {}, 0};
    for (double frac : {0.00001, 0.0001, 0.001, 0.01, 0.05, 0.2, 1.0}) {
        const int nl = static_cast<int>(g.live_leaves.size());
        int k = std::max(1, static_cast<int>(frac * nl));
        std::vector<int> changed;
        for (int j = 0; j < k; ++j) changed.push_back(g.live_leaves[r() % static_cast<uint64_t>(nl)]);
        std::sort(changed.begin(), changed.end());
        changed.erase(std::unique(changed.begin(), changed.end()), changed.end());
        std::vector<double> newvals(changed.size());
        for (auto& x : newvals) x = d(r);
        std::vector<double> tf, ti, ta; size_t recomputed = 0; bool fell_back = false;
        std::vector<double> vf, vi, va;
        const size_t budget = static_cast<size_t>(0.05 * (n - g.leaves));  // crossover budget: 5% of nodes (measured break-even ~15%, marking costs ~4x a node update)
        for (int rep = 0; rep < 5; ++rep) {
            vf = base; vi = base; va = base;
            for (size_t j = 0; j < changed.size(); ++j) vf[changed[j]] = vi[changed[j]] = va[changed[j]] = newvals[j];
            double a = now_ns(); full_recompute(g, vf); tf.push_back(now_ns() - a);
            a = now_ns(); recomputed = incremental(g, changed, vi, st); ti.push_back(now_ns() - a);
            a = now_ns(); adaptive(g, changed, va, st, budget, fell_back); ta.push_back(now_ns() - a);
        }
        std::sort(tf.begin(), tf.end()); std::sort(ti.begin(), ti.end()); std::sort(ta.begin(), ta.end());
        bool same = std::memcmp(vf.data(), vi.data(), sizeof(double) * n) == 0 && std::memcmp(vf.data(), va.data(), sizeof(double) * n) == 0;
        std::fprintf(csv, "A_dependency_graph,%s_adaptive,changed_leaves=%zu,%.0f,%.0f,,%.3f,%s,baseline=full recompute candidate=adaptive crossover (budget 5%%) fell_back=%d\n", topo,
                     changed.size(), tf[2], ta[2], tf[2] / ta[2], same ? "bit_identical" : "MISMATCH", fell_back ? 1 : 0);
        failures += !same;
        double affected = static_cast<double>(recomputed) / (n - g.leaves);
        std::fprintf(csv, "A_dependency_graph,%s,changed_leaves=%zu,%.0f,%.0f,%.6f,%.3f,%s,baseline=full recompute candidate=omega dirty-cone\n", topo, changed.size(), tf[2], ti[2], affected,
                     tf[2] / ti[2], same ? "bit_identical" : "MISMATCH");
        std::printf("A %-22s changed=%7zu affected=%8.4f%% full_ns=%11.0f incr_ns=%11.0f speedup=%8.2fx adaptive_ns=%11.0f adaptive_speedup=%7.2fx%s %s\n", topo, changed.size(),
                    100 * affected, tf[2], ti[2], tf[2] / ti[2], ta[2], tf[2] / ta[2], fell_back ? " (fell back)" : "", same ? "bit_identical" : "MISMATCH");
    }
}

// ---------------- Family C: heat equation light cone ----------------
void family_c(FILE* csv, int& failures) {
    const int N = 32768, T = 512;
    const double rcoef = 0.25;
    std::vector<double> traj(size_t(T + 1) * N, 0.0);
    auto row = [&](std::vector<double>& tr, int t) { return tr.data() + size_t(t) * N; };
    std::mt19937_64 r(11);
    std::uniform_real_distribution<double> d(0, 1);
    for (int i = 1; i < N - 1; ++i) traj[i] = d(r);
    auto step_range = [&](std::vector<double>& tr, int t, int lo, int hi) {  // computes row t from t-1 on [lo, hi]
        const double* p = row(tr, t - 1); double* q = row(tr, t);
        for (int i = lo; i <= hi; ++i) q[i] = p[i] + rcoef * (p[i - 1] - 2.0 * p[i] + p[i + 1]);
    };
    for (int t = 1; t <= T; ++t) step_range(traj, t, 1, N - 2);
    for (int k : {1, 16, 256}) {
        std::vector<int> cells;
        for (int j = 0; j < k; ++j) cells.push_back(1 + static_cast<int>(r() % (N - 2)));
        std::sort(cells.begin(), cells.end());
        std::vector<double> tf, ti; size_t updates = 0;
        std::vector<double> full, inc;
        for (int rep = 0; rep < 3; ++rep) {
            full = traj; inc = traj;
            for (int c : cells) { full[c] += 1e-3; inc[c] += 1e-3; }
            double a = now_ns();
            for (int t = 1; t <= T; ++t) step_range(full, t, 1, N - 2);
            tf.push_back(now_ns() - a);
            a = now_ns();
            updates = 0;
            std::vector<std::pair<int, int>> iv;  // dirty intervals at step t-1
            for (int c : cells) iv.push_back({c, c});
            for (int t = 1; t <= T; ++t) {
                std::vector<std::pair<int, int>> nx;
                for (auto [lo, hi] : iv) {  // Ω.PROP: the cone widens by one cell per step
                    int l = std::max(1, lo - 1), h = std::min(N - 2, hi + 1);
                    if (!nx.empty() && l <= nx.back().second + 1) nx.back().second = std::max(nx.back().second, h);
                    else nx.push_back({l, h});
                }
                for (auto [lo, hi] : nx) { step_range(inc, t, lo, hi); updates += size_t(hi - lo + 1); }
                iv.swap(nx);
            }
            ti.push_back(now_ns() - a);
        }
        std::sort(tf.begin(), tf.end()); std::sort(ti.begin(), ti.end());
        bool same = std::memcmp(full.data(), inc.data(), sizeof(double) * full.size()) == 0;
        failures += !same;
        double affected = static_cast<double>(updates) / (double(T) * (N - 2));
        std::fprintf(csv, "C_heat_equation_1d,N=%d_T=%d,perturbed_cells=%d,%.0f,%.0f,%.6f,%.3f,%s,baseline=full recompute candidate=light-cone\n", N, T, k, tf[1], ti[1], affected,
                     tf[1] / ti[1], same ? "bit_identical" : "MISMATCH");
        std::printf("C heat N=%d T=%d cells=%4d affected=%8.4f%% full_ns=%11.0f incr_ns=%11.0f speedup=%8.2fx %s\n", N, T, k, 100 * affected,
                    tf[1], ti[1], tf[1] / ti[1], same ? "bit_identical" : "MISMATCH");
    }
    std::printf("C trajectory_storage_MB=%.1f (cost of keeping the reusable state)\n", sizeof(double) * traj.size() / 1048576.0);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: omega-bench <csv_out>\n"); return 2; }
    FILE* csv = std::fopen(argv[1], "a");
    if (!csv) { std::perror(argv[1]); return 2; }
    int failures = 0;
    family_a(csv, "partitioned_256", 256, 0, failures);
    family_a(csv, "sliding_window_4096", 1, 4096, failures);
    std::printf("A max_rss_MB=%ld\n", max_rss_mb());
    family_c(csv, failures);
    std::printf("C max_rss_MB=%ld\n", max_rss_mb());
    std::fclose(csv);
    std::printf("BENCH RESULT=%s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
