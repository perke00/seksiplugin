// KITSUNE EQ DSP core tests. Build: g++ -std=c++17 -O2 Tests/dsp_test.cpp -o dsp_test && ./dsp_test
#include "../Source/KitsuneCore.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace kit;
static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const double SR = 48000.0;

static std::vector<float> noise (int n, float sigma, unsigned seed)
{
    std::mt19937 rng (seed); std::normal_distribution<float> d (0.f, sigma);
    std::vector<float> v ((size_t) n); for (auto& x : v) x = d (rng); return v;
}

static void run (Engine& e, std::vector<float>& l, std::vector<float>& r, const Settings& s)
{
    const int block = 441; // awkward block size on purpose
    for (size_t i = 0; i < l.size(); i += block)
    {
        const int n = (int) std::min<size_t> (block, l.size() - i);
        e.process (l.data() + i, r.data() + i, n, s);
    }
}

// average magnitude spectrum (dB) over frames of a segment
static std::vector<double> avgSpectrumDb (const std::vector<float>& x, int start, int frames)
{
    Fft fft (11);
    std::vector<double> acc (kBins, 0.0);
    std::vector<float> re (kFftSize), im (kFftSize);
    for (int f = 0; f < frames; ++f)
    {
        for (int i = 0; i < kFftSize; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (2.f * kPi * (float) i / (float) kFftSize);
            re[(size_t) i] = x[(size_t) (start + f * 1024 + i)] * w; im[(size_t) i] = 0.f;
        }
        fft.transform (re.data(), im.data(), false);
        for (int k = 0; k < kBins; ++k) acc[(size_t) k] += (double) re[(size_t) k] * re[(size_t) k] + (double) im[(size_t) k] * im[(size_t) k];
    }
    for (auto& a : acc) a = 10.0 * std::log10 (a / frames + 1e-18);
    return acc;
}

static double bandMean (const std::vector<double>& s, int k0, int k1)
{
    double a = 0; for (int k = k0; k <= k1; ++k) a += s[(size_t) k]; return a / (k1 - k0 + 1);
}

int main()
{
    // 1) depth 0 = transparent, exactly the input delayed by the latency
    {
        std::printf ("[1] transparency / latency\n");
        Engine e; e.prepare (SR);
        const int n = 48000;
        auto in = noise (n, 0.2f, 1);
        auto l = in, r = in;
        Settings s; s.depth = 0.f;
        run (e, l, r, s);
        double err = 0; for (int i = kFftSize + 4096; i < n; ++i) err = std::max (err, (double) std::fabs (l[(size_t) i] - in[(size_t) (i - kFftSize)]));
        std::printf ("  max error vs delayed input: %.2e\n", err);
        CHECK (err < 1e-4, "not transparent (err %g)", err);
    }

    // 2) mix 0 = dry, bypass = dry, delta at depth 0 = silence
    {
        std::printf ("[2] mix / bypass / delta\n");
        const int n = 24000; auto in = noise (n, 0.2f, 2);
        { Engine e; e.prepare (SR); auto l = in, r = in; Settings s; s.mix = 0.f; run (e, l, r, s);
          double err = 0; for (int i = kFftSize; i < n; ++i) err = std::max (err, (double) std::fabs (l[(size_t) i] - in[(size_t) (i - kFftSize)]));
          CHECK (err < 1e-5, "mix=0 not dry (%g)", err); }
        { Engine e; e.prepare (SR); auto l = in, r = in; Settings s; s.bypass = true; s.depth = 1.f; run (e, l, r, s);
          double err = 0; for (int i = kFftSize + 2000; i < n; ++i) err = std::max (err, (double) std::fabs (l[(size_t) i] - in[(size_t) (i - kFftSize)]));
          CHECK (err < 1e-3, "bypass not dry (%g)", err); }
        { Engine e; e.prepare (SR); auto l = in, r = in; Settings s; s.depth = 0.f; s.delta = true; run (e, l, r, s);
          double mx = 0; for (int i = kFftSize + 4096; i < n; ++i) mx = std::max (mx, (double) std::fabs (l[(size_t) i]));
          CHECK (mx < 1e-4, "delta at depth 0 not silent (%g)", mx); }
    }

    // 3) resonance suppression: noise bed + strong narrow tone
    {
        std::printf ("[3] resonance suppression\n");
        const int n = 48000 * 3; const int toneBin = 128; // 3 kHz at 48k
        const double toneHz = toneBin * SR / kFftSize;
        auto bed = noise (n, 0.1f, 3);
        std::vector<float> in (bed);
        for (int i = 0; i < n; ++i) in[(size_t) i] += 0.1f * (float) std::sin (2.0 * 3.14159265358979 * toneHz * i / SR);

        auto measure = [&] (const Settings& s, double& toneRed, double& floorChange)
        {
            Engine e; e.prepare (SR); auto l = in, r = in; run (e, l, r, s);
            const int start = 2 * 48000;
            auto sIn = avgSpectrumDb (in, start - kFftSize, 40);   // aligned for the latency
            auto sOut = avgSpectrumDb (l, start, 40);
            toneRed = sIn[(size_t) toneBin] - sOut[(size_t) toneBin];
            floorChange = bandMean (sOut, 300, 500) - bandMean (sIn, 300, 500);
        };

        Settings soft; soft.depth = 0.35f;
        double tr, fc; measure (soft, tr, fc);
        std::printf ("  default (soft, depth 35%%): tone -%.1f dB, noise floor %+.2f dB\n", tr, fc);
        CHECK (tr > 4.0, "default too weak (%g dB)", tr);
        CHECK (std::fabs (fc) < 1.0, "default changes the floor too much (%g dB)", fc);

        Settings strong; strong.depth = 0.9f; strong.hardMode = true; strong.sharpness = 0.9f;
        measure (strong, tr, fc);
        std::printf ("  strong (hard, depth 90%%):  tone -%.1f dB, noise floor %+.2f dB\n", tr, fc);
        CHECK (tr > 12.0, "strong setting too weak (%g dB)", tr);
        CHECK (std::fabs (fc) < 2.0, "strong setting damages the floor (%g dB)", fc);

        // tone outside the processing range must be left alone
        Settings ranged; ranged.depth = 0.9f; ranged.hardMode = true; ranged.lowHz = 6000.f; ranged.highHz = 16000.f;
        measure (ranged, tr, fc);
        std::printf ("  range 6k-16k, tone at 3k:   tone -%.1f dB\n", tr);
        CHECK (tr < 1.0, "range ignored (%g dB)", tr);
    }

    // 4) no-noise-bed sanity: pink-ish noise must not be dulled at default settings
    {
        std::printf ("[4] broadband material stays intact\n");
        const int n = 48000 * 3;
        auto white = noise (n, 0.1f, 4);
        std::vector<float> in (white); float z = 0.f; // simple tilt: low-pass one pole
        for (auto& x : in) { z += 0.35f * (x - z); x = z * 2.f; }
        Engine e; e.prepare (SR); auto l = in, r = in; Settings s; run (e, l, r, s);
        const int start = 2 * 48000;
        auto a = avgSpectrumDb (in, start - kFftSize, 40), b = avgSpectrumDb (l, start, 40);
        for (auto band : { std::pair<int,int> {20, 60}, {100, 200}, {300, 500} })
        {
            const double d = bandMean (b, band.first, band.second) - bandMean (a, band.first, band.second);
            std::printf ("  bins %d-%d: %+.2f dB\n", band.first, band.second, d);
            CHECK (std::fabs (d) < 1.5, "tonal balance shifted by %g dB", d);
        }
    }

    // 5) transient guard keeps more of a click
    {
        std::printf ("[5] transient guard\n");
        const int n = 48000 * 2;
        auto bed = noise (n, 0.02f, 5);
        std::vector<float> in (bed);
        for (int i = 0; i < n; ++i) in[(size_t) i] += 0.05f * (float) std::sin (2.0 * 3.14159265358979 * 3000.0 * i / SR);
        const int clickAt = 48000;
        for (int j = 0; j < 8; ++j) in[(size_t) (clickAt + j)] += (j & 1 ? -0.6f : 0.6f);
        auto peakAfter = [&] (float guard)
        {
            Engine e; e.prepare (SR); auto l = in, r = in;
            Settings s; s.depth = 0.9f; s.hardMode = true; s.transientGuard = guard; run (e, l, r, s);
            float p = 0; for (int i = clickAt + kFftSize - 64; i < clickAt + kFftSize + 128; ++i) p = std::max (p, std::fabs (l[(size_t) i])); return p;
        };
        const float p0 = peakAfter (0.f), p1 = peakAfter (1.f);
        std::printf ("  click peak: guard off %.3f, guard on %.3f\n", p0, p1);
        CHECK (p1 >= p0 * 0.98f, "guard made the transient weaker");
    }

    // 6) robustness: random parameters, silence, extreme input - no NaN/inf, bounded
    {
        std::printf ("[6] robustness fuzz\n");
        std::mt19937 rng (99); std::uniform_real_distribution<float> u (0.f, 1.f);
        bool bad = false; float peak = 0.f;
        for (int t = 0; t < 12; ++t)
        {
            Engine e; e.prepare (t % 2 ? 44100.0 : 96000.0);
            Settings s;
            s.depth = u (rng); s.triggerDb = 12.f * u (rng); s.focus = u (rng); s.sharpness = u (rng);
            s.tiltDbPerOct = -3.f + 6.f * u (rng); s.attackMs = 1.f + 99.f * u (rng); s.releaseMs = 10.f + 990.f * u (rng);
            s.transientGuard = u (rng); s.lowHz = 20.f + 1980.f * u (rng); s.highHz = 2000.f + 18000.f * u (rng);
            s.link = u (rng); s.hardMode = u (rng) > 0.5f; s.midSide = u (rng) > 0.5f; s.mix = u (rng);
            s.outputDb = -12.f + 24.f * u (rng); s.delta = u (rng) > 0.8f;
            auto l = noise (48000, 0.5f, (unsigned) t), r = noise (48000, 0.5f, (unsigned) t + 100);
            for (int i = 20000; i < 20100; ++i) { l[(size_t) i] = 1.f; r[(size_t) i] = -1.f; }
            for (int i = 30000; i < 36000; ++i) { l[(size_t) i] = 0.f; r[(size_t) i] = 0.f; }
            run (e, l, r, s);
            for (size_t i = 0; i < l.size(); ++i)
            {
                if (!std::isfinite (l[i]) || !std::isfinite (r[i])) bad = true;
                peak = std::max ({ peak, std::fabs (l[i]), std::fabs (r[i]) });
            }
        }
        std::printf ("  peak output %.2f\n", peak);
        CHECK (!bad, "NaN/inf in output");
        CHECK (peak < 16.f, "output exploded (%g)", peak);
    }

    // 7) mono path + CPU load
    {
        std::printf ("[7] mono + performance\n");
        Engine e; e.prepare (SR);
        auto m = noise ((int) SR * 10, 0.2f, 7); Settings s;
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < m.size(); i += 512) e.process (m.data() + i, nullptr, (int) std::min<size_t> (512, m.size() - i), s);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  10 s of mono audio in %.3f s (%.1f%% of one core)\n", sec, 10.0 * sec);
        CHECK (sec < 3.0, "too slow");

        Engine e2; e2.prepare (SR);
        auto l = noise ((int) SR * 10, 0.2f, 8), r = l;
        const auto t1 = std::chrono::steady_clock::now();
        run (e2, l, r, s);
        const double sec2 = std::chrono::duration<double> (std::chrono::steady_clock::now() - t1).count();
        std::printf ("  10 s of stereo audio in %.3f s (%.1f%% of one core)\n", sec2, 10.0 * sec2);
        CHECK (sec2 < 4.0, "stereo too slow");
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
