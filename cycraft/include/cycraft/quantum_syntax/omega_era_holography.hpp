// ============================================================================
// SINGULAR Ω — ERA-CGH: exact incremental computer-generated holography
// (`DISC-000902`, research). Rule `SG-ARCH-400`: <= 400 physical lines.
//
// Point-source CGH: H(x) = Σ_p a · exp(i k r_p(x)). ERA discipline applied to
// holography: every point's contribution c(p, x) is a PURE function of the
// point's quantized position (picometres, int64) — phase index from IEEE-754
// correctly-rounded double ops (no fast-math, no FP contraction), cos/sin from a
// fixed Q14 table — and contributions accumulate in ℤ (int64). Hence:
//   H_incremental == H_full bit for bit; undo restores H bit for bit; partial
//   holograms from any node partition / order sum to the identical H.
// The fp64 baseline is standard CGH (std::cos/std::sin, double accumulators).
// ============================================================================
#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace cycraft::omega::quantum_syntax::era {

struct HoloSetup {
    int W = 128, H = 128;
    double pitch = 8e-6, lambda = 532e-9, z0 = 0.04;  // pixel pitch, wavelength, object distance (m)
    static constexpr int kLutBits = 12, kAmpQ = 14;    // 4096-entry phase LUT, Q14 amplitude
    double px(int i) const { return (i - W / 2) * pitch; }
    double py(int j) const { return (j - H / 2) * pitch; }
};

struct PointQ { int64_t x, y, z; };  // picometres
inline int64_t to_pm(double metres) { return std::llround(metres * 1e12); }

// Term contract v2 (identical on every path): for point p and pixel (i, j)
//   cyc = sqrt(dx² + dy² + Z²) · (1/λ)      (IEEE-754 double, correctly rounded, no contraction)
//   idx = trunc((cyc − floor(cyc)) · N + 0.5) mod N,   term = (cosQ14[idx], sinQ14[idx])
// A = int64_t: ERA accumulator. A = double: the same quantized terms in fp64 — also
// exact here because every term is an integer and |Σ| < 2^53 (the exactness comes
// from the contract, not from the integer type; kept as an honest control).
template <class A>
class QuantHologram {
public:
    explicit QuantHologram(const HoloSetup& s) : s_(s), re_(static_cast<size_t>(s.W) * s.H, A{}), im_(re_.size(), A{}) {
        const int n = 1 << HoloSetup::kLutBits;
        cos_.resize(n); sin_.resize(n);
        for (int i = 0; i < n; ++i) {
            const double a = 2.0 * M_PI * i / n;
            cos_[i] = static_cast<int32_t>(std::lround(std::cos(a) * (1 << HoloSetup::kAmpQ)));
            sin_[i] = static_cast<int32_t>(std::lround(std::sin(a) * (1 << HoloSetup::kAmpQ)));
        }
        px_.resize(static_cast<size_t>(s.W));
        for (int i = 0; i < s.W; ++i) px_[static_cast<size_t>(i)] = s.px(i);
    }
    // Accelerated path: batch of points with signs (+1 add, −1 retract), rows split
    // over `threads` workers. Each pixel receives the same multiset of integer terms
    // as the reference, so the result is bit-identical for any batch/thread split.
    void apply_points(const PointQ* pts, const int* signs, size_t m, int threads = 1) {
        const int T = std::max(1, std::min(threads, s_.H));
        auto work = [&](int j0, int j1) {
            std::vector<int> idx(static_cast<size_t>(s_.W));
            const double inv_l = 1.0 / s_.lambda, nlut = static_cast<double>(1 << HoloSetup::kLutBits);
            const int mask = (1 << HoloSetup::kLutBits) - 1;
            for (int j = j0; j < j1; ++j) {
                A* re = &re_[static_cast<size_t>(j) * s_.W];
                A* im = &im_[static_cast<size_t>(j) * s_.W];
                const double yj = s_.py(j);
                for (size_t q = 0; q < m; ++q) {
                    const double X = static_cast<double>(pts[q].x) * 1e-12, Y = static_cast<double>(pts[q].y) * 1e-12, Z = static_cast<double>(pts[q].z) * 1e-12;
                    const double dy = yj - Y, dyz = dy * dy + Z * Z;
                    for (int i = 0; i < s_.W; ++i) {  // vectorizable: sqrt, floor, mul, add, convert
                        const double dx = px_[static_cast<size_t>(i)] - X;
                        const double cyc = std::sqrt(dx * dx + dyz) * inv_l;
                        idx[static_cast<size_t>(i)] = static_cast<int>((cyc - std::floor(cyc)) * nlut + 0.5) & mask;
                    }
                    const int sg = signs[q];
                    for (int i = 0; i < s_.W; ++i) { re[i] += sg * cos_[idx[static_cast<size_t>(i)]]; im[i] += sg * sin_[idx[static_cast<size_t>(i)]]; }
                }
            }
        };
        if (T == 1) { work(0, s_.H); return; }
        std::vector<std::thread> pool;
        const int step = (s_.H + T - 1) / T;
        for (int t = 1; t < T; ++t) pool.emplace_back(work, std::min(s_.H, t * step), std::min(s_.H, (t + 1) * step));
        work(0, std::min(s_.H, step));
        for (auto& th : pool) th.join();
    }
    void apply(const PointQ& p, int sign) { apply_points(&p, &sign, 1, 1); }
    // Scalar reference of contract v2 (no precomputation, no batching): identity oracle.
    void apply_reference(const PointQ& p, int sign) {
        const double X = static_cast<double>(p.x) * 1e-12, Y = static_cast<double>(p.y) * 1e-12, Z = static_cast<double>(p.z) * 1e-12;
        for (int j = 0; j < s_.H; ++j)
            for (int i = 0; i < s_.W; ++i) {
                const double dx = s_.px(i) - X, dy = s_.py(j) - Y;
                const double cyc = std::sqrt(dx * dx + (dy * dy + Z * Z)) * (1.0 / s_.lambda);
                const int idx = static_cast<int>((cyc - std::floor(cyc)) * static_cast<double>(1 << HoloSetup::kLutBits) + 0.5) & ((1 << HoloSetup::kLutBits) - 1);
                re_[static_cast<size_t>(j) * s_.W + i] += sign * cos_[idx]; im_[static_cast<size_t>(j) * s_.W + i] += sign * sin_[idx];
            }
    }
    void clear() { std::fill(re_.begin(), re_.end(), A{}); std::fill(im_.begin(), im_.end(), A{}); }
    void full(const std::vector<PointQ>& pts, int threads = 1) {
        clear(); std::vector<int> sg(pts.size(), 1); apply_points(pts.data(), sg.data(), pts.size(), threads);
    }
    template <class B> size_t mismatch(const QuantHologram<B>& o) const {
        size_t c = 0;
        for (size_t k = 0; k < re_.size(); ++k) c += (static_cast<double>(re_[k]) != static_cast<double>(o.re(k))) || (static_cast<double>(im_[k]) != static_cast<double>(o.im(k)));
        return c;
    }
    A re(size_t k) const { return re_[k]; }
    A im(size_t k) const { return im_[k]; }
    std::complex<double> field(size_t k) const {
        return {static_cast<double>(re_[k]) / (1 << HoloSetup::kAmpQ), static_cast<double>(im_[k]) / (1 << HoloSetup::kAmpQ)};
    }
    void add(const QuantHologram& o) { for (size_t k = 0; k < re_.size(); ++k) { re_[k] += o.re_[k]; im_[k] += o.im_[k]; } }
    size_t bytes() const { return (re_.size() + im_.size()) * sizeof(A) + (cos_.size() + sin_.size()) * 4; }
private:
    HoloSetup s_;
    std::vector<A> re_, im_;
    std::vector<int32_t> cos_, sin_;
    std::vector<double> px_;
};
using ExactHologram = QuantHologram<int64_t>;

// Standard fp64 CGH baseline: exact trig, double accumulation.
class Fp64Hologram {
public:
    explicit Fp64Hologram(const HoloSetup& s) : s_(s), f_(static_cast<size_t>(s.W) * s.H) {}
    void apply(const PointQ& p, double sign) {
        const double X = static_cast<double>(p.x) * 1e-12, Y = static_cast<double>(p.y) * 1e-12, Z = static_cast<double>(p.z) * 1e-12;
        const double k = 2.0 * M_PI / s_.lambda, z2 = Z * Z;
        for (int j = 0; j < s_.H; ++j) {
            const double dy = s_.py(j) - Y, dyz = dy * dy + z2;
            std::complex<double>* row = &f_[static_cast<size_t>(j) * s_.W];
            for (int i = 0; i < s_.W; ++i) {
                const double dx = s_.px(i) - X, ph = k * std::sqrt(dx * dx + dyz);
                row[i] += sign * std::complex<double>(std::cos(ph), std::sin(ph));
            }
        }
    }
    void full(const std::vector<PointQ>& pts) { std::fill(f_.begin(), f_.end(), std::complex<double>{}); for (const auto& p : pts) apply(p, 1.0); }
    const std::vector<std::complex<double>>& data() const { return f_; }
    void add(const Fp64Hologram& o) { for (size_t k = 0; k < f_.size(); ++k) f_[k] += o.f_[k]; }
    size_t mismatch(const Fp64Hologram& o) const { size_t c = 0; for (size_t k = 0; k < f_.size(); ++k) c += !(f_[k] == o.f_[k]); return c; }
    double max_abs_diff(const Fp64Hologram& o) const { double m = 0; for (size_t k = 0; k < f_.size(); ++k) m = std::max(m, std::abs(f_[k] - o.f_[k])); return m; }
    size_t bytes() const { return f_.size() * sizeof(std::complex<double>); }
private:
    HoloSetup s_;
    std::vector<std::complex<double>> f_;
};

// Numerical reconstruction (angular spectrum back-propagation) to verify the
// hologram actually images the object. Radix-2 FFT; W, H must be powers of two.
class HoloOptics {
public:
    using C = std::complex<double>;
    static void fft1(std::vector<C>& a, bool inv) {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
        for (size_t len = 2; len <= n; len <<= 1) {
            const double ang = 2 * M_PI / static_cast<double>(len) * (inv ? 1 : -1);
            const C wl(std::cos(ang), std::sin(ang));
            for (size_t i = 0; i < n; i += len) {
                C w(1);
                for (size_t k = 0; k < len / 2; ++k) { const C u = a[i + k], v = a[i + k + len / 2] * w; a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl; }
            }
        }
        if (inv) for (auto& x : a) x /= static_cast<double>(n);
    }
    static void fft2(std::vector<C>& f, int W, int H, bool inv) {
        std::vector<C> row(static_cast<size_t>(W)), col(static_cast<size_t>(H));
        for (int j = 0; j < H; ++j) { std::copy_n(&f[static_cast<size_t>(j) * W], W, row.begin()); fft1(row, inv); std::copy(row.begin(), row.end(), &f[static_cast<size_t>(j) * W]); }
        for (int i = 0; i < W; ++i) {
            for (int j = 0; j < H; ++j) col[static_cast<size_t>(j)] = f[static_cast<size_t>(j) * W + i];
            fft1(col, inv);
            for (int j = 0; j < H; ++j) f[static_cast<size_t>(j) * W + i] = col[static_cast<size_t>(j)];
        }
    }
    // Propagate field by distance d (negative = back towards the object).
    static std::vector<double> reconstruct(std::vector<C> f, const HoloSetup& s, double d) {
        fft2(f, s.W, s.H, false);
        const double k = 2 * M_PI / s.lambda;
        for (int j = 0; j < s.H; ++j)
            for (int i = 0; i < s.W; ++i) {
                const double fx = ((i < s.W / 2) ? i : i - s.W) / (s.W * s.pitch), fy = ((j < s.H / 2) ? j : j - s.H) / (s.H * s.pitch);
                const double kz2 = k * k - 4 * M_PI * M_PI * (fx * fx + fy * fy);
                C& v = f[static_cast<size_t>(j) * s.W + i];
                v = kz2 > 0 ? v * std::polar(1.0, std::sqrt(kz2) * d) : C{};
            }
        fft2(f, s.W, s.H, true);
        std::vector<double> I(f.size());
        for (size_t q = 0; q < f.size(); ++q) I[q] = std::norm(f[q]);
        return I;
    }
    static void write_bmp(const std::string& path, const std::vector<double>& I, int W, int H) {
        double mx = 0; for (double v : I) mx = std::max(mx, v);
        const int rowb = (W * 3 + 3) & ~3; const uint32_t sz = 54 + static_cast<uint32_t>(rowb * H);
        std::ofstream o(path, std::ios::binary);
        auto u32 = [&](uint32_t v) { o.put(static_cast<char>(v)); o.put(static_cast<char>(v >> 8)); o.put(static_cast<char>(v >> 16)); o.put(static_cast<char>(v >> 24)); };
        auto u16 = [&](uint16_t v) { o.put(static_cast<char>(v)); o.put(static_cast<char>(v >> 8)); };
        o.put('B'); o.put('M'); u32(sz); u32(0); u32(54); u32(40); u32(static_cast<uint32_t>(W)); u32(static_cast<uint32_t>(H)); u16(1); u16(24);
        u32(0); u32(static_cast<uint32_t>(rowb * H)); u32(2835); u32(2835); u32(0); u32(0);
        for (int j = 0; j < H; ++j) {
            for (int i = 0; i < W; ++i) {
                const double v = mx > 0 ? std::sqrt(I[static_cast<size_t>(j) * W + i] / mx) : 0;  // amplitude display
                const char c = static_cast<char>(std::lround(std::clamp(v, 0.0, 1.0) * 255));
                o.put(c); o.put(c); o.put(c);
            }
            for (int pad = W * 3; pad < rowb; ++pad) o.put(0);
        }
    }
};

} // namespace cycraft::omega::quantum_syntax::era
