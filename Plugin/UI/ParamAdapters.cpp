#include "ParamAdapters.h"

using namespace juce;

//==============================================================================
SylenthAdapter::SylenthAdapter (VCCloneAudioProcessor& p, int i, String l)
    : proc (p), index (i), label (std::move (l))
{
    int n = 0;
    const char* const* names = vc::sylenth::listItems (i, n);
    const auto& d = vc::sylenth::def (i);
    if (d.steps > 0)
    {
        count = d.steps;
        for (int k = 0; k < count; ++k) list.add (names ? String (names[k]) : String (k));
    }
}

int SylenthAdapter::defaultValue() const
{
    const float d = vc::sylenth::def (index).def;
    return count > 0 ? vc::SylenthEngine::listValue (d, count) : roundToInt (d * 100.0f);
}

int SylenthAdapter::get() const
{
    const float v = proc.getSylenth (index);
    return count > 0 ? vc::SylenthEngine::listValue (v, count) : roundToInt (v * 100.0f);
}

void SylenthAdapter::set (int v)
{
    proc.setSylenth (index, count > 0 ? (count > 1 ? (float) v / (float) (count - 1) : 0.0f) : v / 100.0f);
}

String SylenthAdapter::text (int v) const
{
    if (count > 0) return list[jlimit (0, count - 1, v)];
    return String (v);
}

//==============================================================================
SpireAdapter::SpireAdapter (VCCloneAudioProcessor& p, int i, String l, Kind k, StringArray items)
    : proc (p), index (i), label (std::move (l)), kind (k), list (std::move (items)) {}

int SpireAdapter::minimum() const
{
    switch (kind) { case Kind::Octave: return -4; case Kind::Semitone: return -11; case Kind::Signed: return -50; default: return 0; }
}

int SpireAdapter::maximum() const
{
    switch (kind)
    {
        case Kind::List: return jmax (0, list.size() - 1);
        case Kind::Flag: return 1;
        case Kind::Octave: return 4;
        case Kind::Semitone: return 11;
        case Kind::Signed: return 50;
        default: return 100;
    }
}

int SpireAdapter::decode (float v) const
{
    switch (kind)
    {
        case Kind::List: return jlimit (0, jmax (0, list.size() - 1), vc::SpireEngine::listIndex (v));
        case Kind::Flag: return vc::SpireEngine::flag (v) ? 1 : 0;
        case Kind::Octave: return (int) (v * 8.99f) - 4;
        case Kind::Semitone: return (int) (v * 22.99f) - 11;
        case Kind::Signed: return roundToInt ((v - 0.5f) * 100.0f);
        default: return roundToInt (v * 100.0f);
    }
}

float SpireAdapter::encode (int k) const
{
    switch (kind)
    {
        case Kind::List: return (float) k / 999.0f;
        case Kind::Flag: return k ? 1.0f : 0.0f;
        case Kind::Octave: return ((float) (k + 4) + 0.5f) / 8.99f;
        case Kind::Semitone: return ((float) (k + 11) + 0.5f) / 22.99f;
        case Kind::Signed: return 0.5f + (float) k / 100.0f;
        default: return (float) k / 100.0f;
    }
}

int SpireAdapter::defaultValue() const { return decode (vc::spire::kParams[index].init); }
int SpireAdapter::get() const { return decode (proc.getSpire (index)); }
void SpireAdapter::set (int v) { proc.setSpire (index, encode (jlimit (minimum(), maximum(), v))); }

String SpireAdapter::text (int v) const
{
    switch (kind)
    {
        case Kind::List: return list[jlimit (0, jmax (0, list.size() - 1), v)];
        case Kind::Flag: return v ? "On" : "Off";
        case Kind::Octave: case Kind::Semitone: return (v > 0 ? "+" : "") + String (v);
        case Kind::Signed: return (v > 0 ? "+" : "") + String (v);
        default: return String (v);
    }
}
