#pragma once
// KITSUNE EQ - DSP core. Pure C++17, no JUCE dependency, so it can be unit-tested anywhere.
//
// Signal flow (per channel, STFT 2048 / hop 512, sqrt-Hann analysis + synthesis):
//   input -> window -> FFT -> magnitude(dB) -> temporal detector smoothing
//         -> reference envelope (log-frequency moving average, width = FOCUS)
//         -> excess = spectrum - envelope  -> threshold/knee/ratio (TRIGGER, MODE, DEPTH, TILT)
//         -> band weight (LOW/HIGH) -> transient guard -> stereo link
//         -> notch shaping (SHARPNESS) -> attack/release per bin -> apply gain -> IFFT -> overlap-add
// Latency = kFftSize samples (reported to the host).

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace kit
{
constexpr int kFftSize = 2048;
constexpr int kHop = 512;
constexpr int kBins = kFftSize / 2 + 1;
constexpr int kDisplayPoints = 256;
constexpr float kPi = 3.14159265358979f;
constexpr float kDetNew = 0.35f;   // detector smoothing: weight of the newest frame (about 3 frames of averaging)

inline constexpr int getLatencySamplesStatic() { return kFftSize; }

struct Settings
{
    float depth = 0.35f;          // 0..1   overall amount of suppression
    float triggerDb = 4.0f;       // 0..12  how far a peak must stick out before it is touched
    float focus = 0.5f;           // 0..1   width of the reference envelope (narrow..wide)
    float sharpness = 0.5f;       // 0..1   0 = broad smooth cuts, 1 = narrow surgical cuts
    float tiltDbPerOct = 0.f;     // -3..3  positive = more sensitive towards the highs
    float attackMs = 8.f;         // 1..100
    float releaseMs = 120.f;      // 10..1000
    float transientGuard = 0.5f;  // 0..1   protects onsets from being dulled
    float lowHz = 120.f;          // processing range
    float highHz = 16000.f;
    float link = 1.f;             // 0..1   stereo link of the reduction curve
    bool hardMode = false;        // false = soft (gentle knee/ratio), true = hard
    bool midSide = false;         // process mid/side instead of left/right
    float mix = 1.f;              // 0..1
    float outputDb = 0.f;         // -12..12
    bool delta = false;           // listen to what is being removed
    bool bypass = false;
};

// ------------------------------------------------------------------ FFT
class Fft
{
public:
    explicit Fft (int order) : n (1 << order), cosT ((size_t) (1 << order) / 2), sinT ((size_t) (1 << order) / 2), rev ((size_t) (1 << order))
    {
        for (int i = 0; i < n / 2; ++i)
        {
            cosT[(size_t) i] = (float) std::cos (2.0 * 3.14159265358979323846 * i / n);
            sinT[(size_t) i] = (float) std::sin (2.0 * 3.14159265358979323846 * i / n);
        }
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
    }

    // in-place complex transform; inverse is scaled by 1/n
    void transform (float* re, float* im, bool inverse) const
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (j > i) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const float wr = cosT[(size_t) (j * step)];
                    const float wi = inverse ? sinT[(size_t) (j * step)] : -sinT[(size_t) (j * step)];
                    const int a = i + j, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi;
                    const float ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti;
                    re[a] += tr;        im[a] += ti;
                }
        }
        if (inverse)
        {
            const float s = 1.f / (float) n;
            for (int i = 0; i < n; ++i) { re[i] *= s; im[i] *= s; }
        }
    }

private:
    int n;
    std::vector<float> cosT, sinT;
    std::vector<int> rev;
};

// ------------------------------------------------------------------ Engine
class Engine
{
public:
    Engine() : fft (11)
    {
        window.resize (kFftSize);
        for (int i = 0; i < kFftSize; ++i)
            window[(size_t) i] = std::sqrt (0.5f - 0.5f * std::cos (2.f * kPi * (float) i / (float) kFftSize)); // sqrt(periodic Hann)
        for (auto& c : ch) c.init();
        for (auto& d : displayIn) d.store (-120.f);
        for (auto& d : displayGr) d.store (0.f);
        prepare (44100.0);
    }

    static int getLatencySamples() { return kFftSize; }

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        reset();
        // display mapping: log-spaced 20 Hz .. 20 kHz
        for (int i = 0; i < kDisplayPoints; ++i)
        {
            const double f0 = 20.0 * std::pow (1000.0, (double) i / kDisplayPoints);
            const double f1 = 20.0 * std::pow (1000.0, (double) (i + 1) / kDisplayPoints);
            const double binHz = sr / (double) kFftSize;
            int b0 = (int) std::floor (f0 / binHz), b1 = (int) std::ceil (f1 / binHz);
            b0 = std::clamp (b0, 1, kBins - 2);
            b1 = std::clamp (b1, b0 + 1, kBins - 1);
            dispLo[(size_t) i] = b0; dispHi[(size_t) i] = b1;
        }
    }

    void reset()
    {
        for (auto& c : ch) c.clear();
        hopCounter = 0; mixSm = 1.f; gainSm = 1.f; bypassSm = 0.f; firstBlock = true;
        guardState = 1.f;
    }

    // In-place. r may be nullptr for mono.
    void process (float* l, float* r, int numSamples, const Settings& s)
    {
        const int nCh = (r != nullptr) ? 2 : 1;
        cfg = s;
        const float mixT = s.mix, gainT = std::pow (10.f, s.outputDb / 20.f), byT = s.bypass ? 1.f : 0.f;
        if (firstBlock) { mixSm = mixT; gainSm = gainT; bypassSm = byT; firstBlock = false; }
        const float sm = 0.004f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = l[i], inR = nCh == 2 ? r[i] : inL;

            // dry path, delayed by the engine latency
            const float dryL = ch[0].dry[(size_t) dryPos], dryR = ch[1].dry[(size_t) dryPos];
            ch[0].dry[(size_t) dryPos] = inL;
            ch[1].dry[(size_t) dryPos] = inR;

            float x0 = inL, x1 = inR;
            if (s.midSide && nCh == 2) { x0 = 0.5f * (inL + inR); x1 = 0.5f * (inL - inR); }

            ch[0].inFifo[(size_t) pos] = x0;
            ch[1].inFifo[(size_t) pos] = x1;
            float w0 = ch[0].outAccum[(size_t) pos]; ch[0].outAccum[(size_t) pos] = 0.f;
            float w1 = ch[1].outAccum[(size_t) pos]; ch[1].outAccum[(size_t) pos] = 0.f;

            pos = (pos + 1) & (kFftSize - 1);
            dryPos = (dryPos + 1) & (kFftSize - 1);

            if (++hopCounter == kHop) { hopCounter = 0; processFrame (nCh); }

            float wetL = w0, wetR = w1;
            if (s.midSide && nCh == 2) { wetL = w0 + w1; wetR = w0 - w1; }

            mixSm += (mixT - mixSm) * sm; gainSm += (gainT - gainSm) * sm; bypassSm += (byT - bypassSm) * sm;

            auto mixOne = [&] (float dry, float wet)
            {
                float o = s.delta ? (dry - wet) : (dry + (wet - dry) * mixSm);
                o *= gainSm;
                return o + (dry - o) * bypassSm;
            };
            l[i] = mixOne (dryL, wetL);
            if (nCh == 2) r[i] = mixOne (dryR, wetR);
        }
    }

    // display data (written on the audio thread, read on the UI thread)
    float getDisplayIn (int i) const { return displayIn[(size_t) i].load (std::memory_order_relaxed); }
    float getDisplayGr (int i) const { return displayGr[(size_t) i].load (std::memory_order_relaxed); }
    float getSampleRate() const { return sr; }

private:
    struct Channel
    {
        std::vector<float> inFifo, outAccum, dry, re, im;
        std::array<float, kBins> magDb {}, prevDb {}, detDb {}, target {}, gain {}, tmp {};
        void init()
        {
            inFifo.assign (kFftSize, 0.f); outAccum.assign (kFftSize, 0.f); dry.assign (kFftSize, 0.f);
            re.assign (kFftSize, 0.f); im.assign (kFftSize, 0.f);
            clear();
        }
        void clear()
        {
            std::fill (inFifo.begin(), inFifo.end(), 0.f);
            std::fill (outAccum.begin(), outAccum.end(), 0.f);
            std::fill (dry.begin(), dry.end(), 0.f);
            magDb.fill (-120.f); prevDb.fill (-120.f); detDb.fill (-120.f);
            target.fill (0.f); gain.fill (0.f); tmp.fill (0.f);
        }
    };

    static float smooth01 (float t) { t = std::clamp (t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }

    void processFrame (int nCh)
    {
        const float binHz = sr / (float) kFftSize;

        // 1) analysis
        float fluxMax = 0.f;
        for (int c = 0; c < nCh; ++c)
        {
            auto& C = ch[(size_t) c];
            for (int i = 0; i < kFftSize; ++i)
            {
                C.re[(size_t) i] = C.inFifo[(size_t) ((pos + i) & (kFftSize - 1))] * window[(size_t) i];
                C.im[(size_t) i] = 0.f;
            }
            fft.transform (C.re.data(), C.im.data(), false);

            float flux = 0.f; int cnt = 0;
            for (int k = 0; k < kBins; ++k)
            {
                const float p = C.re[(size_t) k] * C.re[(size_t) k] + C.im[(size_t) k] * C.im[(size_t) k];
                const float db = 10.f * std::log10 (p + 1e-18f);
                C.magDb[(size_t) k] = db;
                // detector: light power-domain smoothing over time (about 2 frames)
                const float prevP = std::pow (10.f, C.detDb[(size_t) k] * 0.1f);
                const float dp = (1.f - kDetNew) * prevP + kDetNew * p;
                C.detDb[(size_t) k] = 10.f * std::log10 (dp + 1e-18f);
                // spectral flux for the transient guard (only audible bins)
                if (db > -60.f && k > 2 && k < kBins * 3 / 4)
                {
                    flux += std::max (0.f, db - C.prevDb[(size_t) k]);
                    ++cnt;
                }
                C.prevDb[(size_t) k] = db;
            }
            fluxMax = std::max (fluxMax, cnt > 0 ? flux / (float) cnt : 0.f);
        }

        // transient guard: strong broadband onsets reduce the amount of suppression for a moment
        const float onset = std::clamp ((fluxMax - 3.f) / 9.f, 0.f, 1.f);
        const float guardTarget = 1.f - cfg.transientGuard * onset;
        guardState = guardTarget < guardState ? guardTarget : guardState + (guardTarget - guardState) * 0.25f;

        // 2) reduction targets
        for (int c = 0; c < nCh; ++c) computeTarget (ch[(size_t) c], binHz);

        // 3) stereo link
        if (nCh == 2 && cfg.link > 0.f)
        {
            for (int k = 0; k < kBins; ++k)
            {
                const float a = ch[0].target[(size_t) k], b = ch[1].target[(size_t) k], m = std::min (a, b);
                ch[0].target[(size_t) k] = a + (m - a) * cfg.link;
                ch[1].target[(size_t) k] = b + (m - b) * cfg.link;
            }
        }

        // 4) shaping, time smoothing, apply, resynthesis
        const float hopSec = (float) kHop / sr;
        const float cA = std::exp (-hopSec / (std::max (0.5f, cfg.attackMs) * 0.001f));
        const float cR = std::exp (-hopSec / (std::max (5.f, cfg.releaseMs) * 0.001f));

        for (int c = 0; c < nCh; ++c)
        {
            auto& C = ch[(size_t) c];
            shapeTarget (C);

            for (int k = 0; k < kBins; ++k)
            {
                float& g = C.gain[(size_t) k];
                const float t = C.target[(size_t) k];
                g = t + (g - t) * (t < g ? cA : cR);
                if (std::fabs (g) < 1e-4f) g = 0.f;
            }

            // re/im still hold the spectrum of this frame
            for (int k = 0; k < kBins; ++k)
            {
                const float lin = std::exp (C.gain[(size_t) k] * 0.115129255f);
                C.re[(size_t) k] *= lin; C.im[(size_t) k] *= lin;
                if (k > 0 && k < kBins - 1)
                {
                    C.re[(size_t) (kFftSize - k)] = C.re[(size_t) k];
                    C.im[(size_t) (kFftSize - k)] = -C.im[(size_t) k];
                }
            }
            fft.transform (C.re.data(), C.im.data(), true);
            for (int i = 0; i < kFftSize; ++i)
                C.outAccum[(size_t) ((pos + i) & (kFftSize - 1))] += C.re[(size_t) i] * window[(size_t) i] * 0.5f;
        }

        // 5) display (every second frame)
        if ((++frameCount & 1) == 0)
        {
            for (int i = 0; i < kDisplayPoints; ++i)
            {
                float mx = -120.f, mn = 0.f;
                for (int k = dispLo[(size_t) i]; k <= dispHi[(size_t) i]; ++k)
                {
                    mx = std::max (mx, ch[0].detDb[(size_t) k]);
                    mn = std::min (mn, ch[0].gain[(size_t) k]);
                }
                displayIn[(size_t) i].store (mx, std::memory_order_relaxed);
                displayGr[(size_t) i].store (mn, std::memory_order_relaxed);
            }
        }
    }

    void computeTarget (Channel& C, float binHz)
    {
        // reference envelope = moving average (in dB) with a width that grows with frequency
        const float frac = 0.08f + 0.42f * cfg.focus;
        prefix[0] = 0.0;
        for (int k = 0; k < kBins; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + (double) C.detDb[(size_t) k];

        const float knee = cfg.hardMode ? 2.f : 8.f;
        const float ratio = cfg.hardMode ? 1.f : 0.6f;
        const float scale = 2.f * cfg.depth;
        const float maxCut = 24.f * cfg.depth;

        for (int k = 0; k < kBins; ++k)
        {
            const float f = std::max (1.f, (float) k * binHz);
            // band weight
            const float wl = smooth01 (std::log2 (f / std::max (20.f, cfg.lowHz)) + 0.5f);
            const float wh = smooth01 (std::log2 (std::max (1000.f, cfg.highHz) / f) + 0.5f);
            const float w = wl * wh;
            if (w <= 0.f || cfg.depth <= 0.f || k < 2) { C.target[(size_t) k] = 0.f; continue; }

            const int hw = std::max (3, (int) ((float) k * frac));
            const int a = std::max (0, k - hw), b = std::min (kBins - 1, k + hw);
            const float env = (float) ((prefix[(size_t) b + 1] - prefix[(size_t) a]) / (double) (b - a + 1));
            const float excess = C.detDb[(size_t) k] - env;

            const float thr = std::max (0.f, cfg.triggerDb - cfg.tiltDbPerOct * std::log2 (f / 1000.f));
            const float over = excess - thr;
            float eff;
            if (over <= -0.5f * knee) eff = 0.f;
            else if (over >= 0.5f * knee) eff = over;
            else { const float q = over + 0.5f * knee; eff = q * q / (2.f * knee); }

            float red = std::min (maxCut, eff * ratio * scale);
            red *= w * guardState;
            C.target[(size_t) k] = -red;
        }
    }

    // sharpness: dilate (keep the depth) then smooth (avoid musical noise); width grows with frequency
    void shapeTarget (Channel& C)
    {
        const float rel = 0.10f - 0.09f * cfg.sharpness;   // fraction of the bin index
        auto& t = C.target; auto& tmp = C.tmp;
        for (int k = 0; k < kBins; ++k)
        {
            const int r = std::max (1, (int) ((float) k * rel * 0.25f));
            float m = t[(size_t) k];
            for (int j = std::max (0, k - r); j <= std::min (kBins - 1, k + r); ++j) m = std::min (m, t[(size_t) j]);
            tmp[(size_t) k] = m;
        }
        prefix[0] = 0.0;
        for (int k = 0; k < kBins; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + (double) tmp[(size_t) k];
        for (int k = 0; k < kBins; ++k)
        {
            const int r = std::max (1, (int) ((float) k * rel * 0.5f));
            const int a = std::max (0, k - r), b = std::min (kBins - 1, k + r);
            t[(size_t) k] = (float) ((prefix[(size_t) b + 1] - prefix[(size_t) a]) / (double) (b - a + 1));
        }
    }

    Fft fft;
    std::vector<float> window;
    std::array<Channel, 2> ch;
    std::array<double, kBins + 1> prefix {};
    std::array<int, kDisplayPoints> dispLo {}, dispHi {};
    std::array<std::atomic<float>, kDisplayPoints> displayIn, displayGr;
    Settings cfg;
    float sr = 44100.f;
    int pos = 0, dryPos = 0, hopCounter = 0;
    uint32_t frameCount = 0;
    float mixSm = 1.f, gainSm = 1.f, bypassSm = 0.f, guardState = 1.f;
    bool firstBlock = true;
};
} // namespace kit
