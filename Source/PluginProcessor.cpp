#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using StrFn = std::function<juce::String (float, int)>;
using ValFn = std::function<float (const juce::String&)>;

float parseNumber (const juce::String& t)
{
    float v = t.getFloatValue();
    if (t.containsIgnoreCase ("k")) v *= 1000.f;
    return v;
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const char* name, float lo, float hi, float def,
                                                      float skew, StrFn str, ValFn val)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, 0.f, skew), def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (str)).withValueFromStringFunction (std::move (val)));
}

std::unique_ptr<juce::AudioParameterFloat> makePercent (const char* id, const char* name, float def)
{
    return makeFloat (id, name, 0.f, 1.f, def, 1.f,
                      [] (float v, int) { return juce::String (juce::roundToInt (v * 100.f)) + " %"; },
                      [] (const juce::String& t) { return t.getFloatValue() / 100.f; });
}

juce::String hzText (float v) { return v >= 1000.f ? juce::String (v / 1000.f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }

struct Preset
{
    const char* name;
    float depth, trigger, focus, sharp, tilt, attack, release, guard, low, high;
    bool hard;
};

const Preset kPresets[] = {
    { "Init - Transparent",    0.35f, 4.0f, 0.50f, 0.50f,  0.0f,  8.f, 120.f, 0.50f,  120.f, 16000.f, false },
    { "Vocal Smooth",          0.45f, 3.5f, 0.55f, 0.45f,  0.5f,  6.f, 140.f, 0.60f, 1200.f, 12000.f, false },
    { "Harsh Synth Tamer",     0.60f, 3.0f, 0.60f, 0.60f,  1.0f, 10.f, 200.f, 0.40f,  300.f, 16000.f, false },
    { "Bright Hats",           0.50f, 4.0f, 0.40f, 0.70f,  1.5f,  3.f,  80.f, 0.70f, 4000.f, 18000.f, false },
    { "Distorted Guitar",      0.55f, 3.0f, 0.65f, 0.50f,  0.5f,  8.f, 150.f, 0.50f, 1500.f,  9000.f, false },
    { "Surgical Resonances",   0.80f, 5.0f, 0.35f, 0.90f,  0.0f,  4.f, 100.f, 0.50f,   80.f, 16000.f, true  },
};
}

KitsuneProcessor::KitsuneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KITSUNE_STATE", createLayout())
{
    setLatencySamples (kit::getLatencySamplesStatic());
}

juce::AudioProcessorValueTreeState::ParameterLayout KitsuneProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makePercent (ids::depth, "Depth", 0.35f));
    layout.add (makeFloat (ids::trigger, "Trigger", 0.f, 12.f, 4.f, 1.f,
                           [] (float v, int) { return juce::String (v, 1) + " dB"; },
                           [] (const juce::String& t) { return t.getFloatValue(); }));
    layout.add (makePercent (ids::focus, "Focus", 0.5f));
    layout.add (makePercent (ids::sharpness, "Sharpness", 0.5f));
    layout.add (makeFloat (ids::tilt, "Tilt", -3.f, 3.f, 0.f, 1.f,
                           [] (float v, int) { return (v > 0.f ? "+" : "") + juce::String (v, 1) + " dB/oct"; },
                           [] (const juce::String& t) { return t.getFloatValue(); }));
    layout.add (makeFloat (ids::attack, "Attack", 1.f, 100.f, 8.f, 0.5f,
                           [] (float v, int) { return juce::String (v, 1) + " ms"; },
                           [] (const juce::String& t) { return t.getFloatValue(); }));
    layout.add (makeFloat (ids::release, "Release", 10.f, 1000.f, 120.f, 0.4f,
                           [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; },
                           [] (const juce::String& t) { return t.getFloatValue(); }));
    layout.add (makePercent (ids::guard, "Transient Guard", 0.5f));
    layout.add (makeFloat (ids::lowHz, "Range Low", 20.f, 2000.f, 120.f, 0.35f,
                           [] (float v, int) { return hzText (v); }, [] (const juce::String& t) { return parseNumber (t); }));
    layout.add (makeFloat (ids::highHz, "Range High", 2000.f, 20000.f, 16000.f, 0.5f,
                           [] (float v, int) { return hzText (v); }, [] (const juce::String& t) { return parseNumber (t); }));
    layout.add (makePercent (ids::link, "Stereo Link", 1.f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::mode, 1 }, "Mode",
                                                              juce::StringArray { "Soft", "Hard" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::stereo, 1 }, "Stereo Mode",
                                                              juce::StringArray { "Left / Right", "Mid / Side" }, 0));
    layout.add (makePercent (ids::mix, "Mix", 1.f));
    layout.add (makeFloat (ids::output, "Output", -12.f, 12.f, 0.f, 1.f,
                           [] (float v, int) { return (v > 0.f ? "+" : "") + juce::String (v, 1) + " dB"; },
                           [] (const juce::String& t) { return t.getFloatValue(); }));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::delta, 1 }, "Delta", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::bypass, 1 }, "Bypass", false));
    return layout;
}

kit::Settings KitsuneProcessor::readSettings() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    kit::Settings s;
    s.depth = get (ids::depth);
    s.triggerDb = get (ids::trigger);
    s.focus = get (ids::focus);
    s.sharpness = get (ids::sharpness);
    s.tiltDbPerOct = get (ids::tilt);
    s.attackMs = get (ids::attack);
    s.releaseMs = get (ids::release);
    s.transientGuard = get (ids::guard);
    s.lowHz = get (ids::lowHz);
    s.highHz = get (ids::highHz);
    s.link = get (ids::link);
    s.hardMode = get (ids::mode) > 0.5f;
    s.midSide = get (ids::stereo) > 0.5f;
    s.mix = get (ids::mix);
    s.outputDb = get (ids::output);
    s.delta = get (ids::delta) > 0.5f;
    s.bypass = get (ids::bypass) > 0.5f;
    return s;
}

void KitsuneProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (kit::getLatencySamplesStatic());
}

bool KitsuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return out == layouts.getMainInputChannelSet();
}

void KitsuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    if (buffer.getNumChannels() == 0) return;
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    engine.process (l, r, buffer.getNumSamples(), readSettings());
}

juce::AudioProcessorEditor* KitsuneProcessor::createEditor() { return new KitsuneEditor (*this); }

int KitsuneProcessor::getNumPrograms() { return (int) (sizeof (kPresets) / sizeof (kPresets[0])); }
const juce::String KitsuneProcessor::getProgramName (int index) { return juce::String (kPresets[juce::jlimit (0, getNumPrograms() - 1, index)].name); }

void KitsuneProcessor::setParam (const char* id, float realValue)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

void KitsuneProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, getNumPrograms() - 1, index);
    currentProgram = index;
    const auto& p = kPresets[index];
    setParam (ids::depth, p.depth);       setParam (ids::trigger, p.trigger);
    setParam (ids::focus, p.focus);       setParam (ids::sharpness, p.sharp);
    setParam (ids::tilt, p.tilt);         setParam (ids::attack, p.attack);
    setParam (ids::release, p.release);   setParam (ids::guard, p.guard);
    setParam (ids::lowHz, p.low);         setParam (ids::highHz, p.high);
    if (auto* m = apvts.getParameter (ids::mode)) m->setValueNotifyingHost (p.hard ? 1.f : 0.f);
}

void KitsuneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void KitsuneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) vt.getProperty ("program", 0);
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KitsuneProcessor(); }
