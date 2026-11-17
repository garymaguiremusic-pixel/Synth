#include "mm/Formats.h"

#include <algorithm>

namespace mm
{
    //==============================================================================
    // Phrase helpers
    int Phrase::lengthTicks() const noexcept
    {
        if (! autoSize)
            return std::max (1, totalSize);

        int lastEnd = 0;
        for (auto& n : notes)
            lastEnd = std::max (lastEnd, n.end);
        const int bars = std::max (1, (lastEnd + kTicksPerBar - 1) / kTicksPerBar);
        return bars * kTicksPerBar;
    }

    int Phrase::rowsPerLane() const noexcept
    {
        int rows = 1;
        for (auto& g : numberGroups)
            rows = std::max (rows, g.second);
        return std::clamp (rows, 1, kMaxRows);
    }

    int Phrase::lanesShown() const noexcept
    {
        const int n = (int) numberGroups.size();
        return n >= 5 ? 5 : (n >= 3 ? 3 : 1);
    }

    void Phrase::setLayout (int lanes, int rows)
    {
        lanes = lanes >= 5 ? 5 : (lanes >= 3 ? 3 : 1);
        rows = std::clamp (rows, 1, kMaxRows);
        numberGroups.clear();
        for (int id = 1; id <= lanes; ++id)
            numberGroups.emplace_back (id, rows);
    }

    void Phrase::sortNotes()
    {
        std::stable_sort (notes.begin(), notes.end(), [] (const PhraseNote& a, const PhraseNote& b)
        {
            return a.start != b.start ? a.start < b.start : a.number < b.number;
        });
    }

    //==============================================================================
    std::vector<int> KeyMap::expand (int key) const
    {
        std::vector<int> out;
        if (key < 0 || key > 127)
            return out;
        const int pc = key % 12, base = key - pc;
        for (int off : map[(size_t) pc])
        {
            const int n = base + off;
            if (n >= 0 && n <= 127 && std::find (out.begin(), out.end(), n) == out.end())
                out.push_back (n);
        }
        std::sort (out.begin(), out.end());
        return out;
    }

    bool KeyMap::isEmpty() const noexcept
    {
        for (auto& m : map)
            if (! m.empty())
                return false;
        return true;
    }

    //==============================================================================
    std::string fileTypeOf (const std::vector<vt::Tree>& trees)
    {
        for (auto& t : trees)
            if (t.type == "HEADER")
                return t.getString ("File_Type");
        return {};
    }

    static const vt::Tree* findTop (const std::vector<vt::Tree>& trees, const char* type)
    {
        for (auto& t : trees)
            if (t.type == type)
                return &t;
        return nullptr;
    }

    Phrase phraseFromTrees (const std::vector<vt::Tree>& trees)
    {
        Phrase p;
        auto* seq = findTop (trees, "SECUENCIA");
        if (seq == nullptr)
            throw vt::ParseError ("no SECUENCIA tree");

        p.name = seq->getString ("Name", "Phrase");
        p.metrica = seq->getString ("Metrica", "4/4");
        p.snap = seq->getString ("Snap", "Auto");
        p.ppq = seq->getInt ("PPQ", kPhrasePpq);
        p.autoSize = seq->getBool ("Auto_Update_Total_Size", true);
        p.totalSize = seq->getInt ("Total_Size", kTicksPerBar);
        p.numberGroups.clear();

        // Rescale if a file ever uses another resolution.
        const double scale = p.ppq > 0 ? (double) kPhrasePpq / (double) p.ppq : 1.0;
        auto ticks = [scale] (int t) { return scale == 1.0 ? t : (int) (t * scale + 0.5); };
        p.totalSize = ticks (p.totalSize);

        for (auto& c : seq->children)
        {
            if (c.type == "Number_Groups")
            {
                p.numberGroups.emplace_back (c.getInt ("Id"), c.getInt ("Value", 3));
            }
            else if (c.type == "Note_Info")
            {
                PhraseNote n;
                n.number = c.getInt ("Number", 50);
                n.noteOffset = c.getInt ("Note_Offset", 0);
                n.velocity = c.getInt ("Velocity", 100);
                n.start = ticks (c.getInt ("Time_Start", 0));
                n.end = ticks (c.getInt ("Time_End", n.start + 24));
                p.notes.push_back (n);
            }
        }
        p.ppq = kPhrasePpq;
        if (p.numberGroups.empty())
            p.setLayout (3, 3);
        return p;
    }

    std::vector<vt::Tree> phraseToTrees (const Phrase& p)
    {
        using vt::Var;
        vt::Tree header ("HEADER");
        header.set ("Version", Var::ofInt (1)).set ("File_Type", Var::ofString ("Secuencia Frases"));

        vt::Tree seq ("SECUENCIA");
        seq.set ("Version", Var::ofInt (2))
           .set ("Type", Var::ofInt (1))
           .set ("Name", Var::ofString (p.name))
           .set ("Metrica", Var::ofString (p.metrica))
           .set ("Snap", Var::ofString (p.snap))
           .set ("PPQ", Var::ofInt (kPhrasePpq))
           .set ("Auto_Update_Total_Size", Var::ofBool (p.autoSize));
        if (! p.autoSize)
            seq.set ("Total_Size", Var::ofInt (p.totalSize));

        for (auto& g : p.numberGroups)
        {
            vt::Tree t ("Number_Groups");
            t.set ("Id", Var::ofInt (g.first)).set ("Value", Var::ofInt (g.second));
            seq.add (std::move (t));
        }
        for (auto& n : p.notes)
        {
            vt::Tree t ("Note_Info");
            t.set ("Version", Var::ofInt (2))
             .set ("Number", Var::ofInt (n.number))
             .set ("Note_Offset", Var::ofInt (n.noteOffset))
             .set ("Velocity", Var::ofInt (n.velocity))
             .set ("Time_Start", Var::ofInt (n.start))
             .set ("Time_End", Var::ofInt (n.end));
            seq.add (std::move (t));
        }
        return { header, seq };
    }

    //==============================================================================
    Groove grooveFromTrees (const std::vector<vt::Tree>& trees)
    {
        auto* q = findTop (trees, "QUANTIZER");
        if (q == nullptr)
            throw vt::ParseError ("no QUANTIZER tree");

        Groove g;
        g.name = q->getString ("Name");
        g.totalPpq = q->getInt ("Total_PPQ", 384);
        g.globalPpq = q->getInt ("Global_PPQ", 96);
        if (auto* nodes = q->child ("Nodes_Tree"))
            for (auto& n : nodes->children)
                g.grid.push_back (n.getInt ("Value"));
        std::sort (g.grid.begin(), g.grid.end());
        return g;
    }

    std::vector<vt::Tree> grooveToTrees (const Groove& g)
    {
        using vt::Var;
        vt::Tree header ("HEADER");
        header.set ("File_Type", Var::ofString ("Quantizer Pattern"));

        vt::Tree q ("QUANTIZER");
        q.set ("Name", Var::ofString (g.name)).set ("Total_PPQ", Var::ofInt (g.totalPpq)).set ("Global_PPQ", Var::ofInt (g.globalPpq));
        vt::Tree nodes ("Nodes_Tree");
        for (int v : g.grid)
        {
            vt::Tree n ("USA_IE");
            n.set ("Value", Var::ofInt (v));
            nodes.add (std::move (n));
        }
        q.add (std::move (nodes));
        return { header, q };
    }

    //==============================================================================
    KeyMap keyMapFromTrees (const std::vector<vt::Tree>& trees)
    {
        auto* h = findTop (trees, "HARMONIZER");
        if (h == nullptr)
            throw vt::ParseError ("no HARMONIZER tree");

        KeyMap km;
        km.extraOctaves = h->getInt ("Number_Extra_Octaves", 1);
        int o = 0;
        for (auto& oct : h->children)
        {
            if (oct.type != "Octave")
                continue;
            for (size_t i = 0; i < oct.children.size() && i < 144; ++i)
                if (oct.children[i].getBool ("Value"))
                    km.map[i / 12].push_back ((o - km.extraOctaves) * 12 + (int) (i % 12));
            ++o;
        }
        for (auto& m : km.map)
            std::sort (m.begin(), m.end());
        return km;
    }

    std::vector<vt::Tree> keyMapToTrees (const KeyMap& km)
    {
        using vt::Var;
        vt::Tree header ("HEADER");
        header.set ("Version", Var::ofInt (1)).set ("File_Type", Var::ofString ("Harmonizer"));

        vt::Tree h ("HARMONIZER");
        h.set ("Number_Extra_Octaves", Var::ofInt (km.extraOctaves));
        const int X = km.extraOctaves;
        for (int o = 0; o < 2 * X + 1; ++o)
        {
            vt::Tree oct ("Octave");
            for (int i = 0; i < 144; ++i)
            {
                const int pc = i / 12, col = i % 12;
                const int off = (o - X) * 12 + col;
                const auto& m = km.map[(size_t) pc];
                vt::Tree node ("Node");
                node.set ("Value", Var::ofBool (std::find (m.begin(), m.end(), off) != m.end()));
                oct.add (std::move (node));
            }
            h.add (std::move (oct));
        }
        return { header, h };
    }

    //==============================================================================
    std::string baseName (const std::string& path)
    {
        auto slash = path.find_last_of ("/\\");
        auto name = slash == std::string::npos ? path : path.substr (slash + 1);
        auto dot = name.find_last_of ('.');
        return dot == std::string::npos ? name : name.substr (0, dot);
    }

    template <typename T, typename Fn>
    static bool loadWith (const std::string& path, T& out, std::string* error, Fn&& fn)
    {
        try
        {
            const auto bytes = vt::readFileBytes (path);
            if (bytes.empty())
            {
                if (error) *error = "cannot read " + path;
                return false;
            }
            out = fn (vt::read (bytes));
            return true;
        }
        catch (const std::exception& e)
        {
            if (error) *error = e.what();
            return false;
        }
    }

    bool loadPhraseFile (const std::string& path, Phrase& p, std::string* error)  { return loadWith (path, p, error, phraseFromTrees); }
    bool loadGrooveFile (const std::string& path, Groove& g, std::string* error)  { return loadWith (path, g, error, grooveFromTrees); }

    bool loadKeyMapFile (const std::string& path, KeyMap& k, std::string* error)
    {
        const bool ok = loadWith (path, k, error, keyMapFromTrees);
        if (ok) k.name = baseName (path);
        return ok;
    }

    bool savePhraseFile (const std::string& path, const Phrase& p)   { return vt::writeFileBytes (path, vt::write (phraseToTrees (p))); }
    bool saveGrooveFile (const std::string& path, const Groove& g)   { return vt::writeFileBytes (path, vt::write (grooveToTrees (g))); }
    bool saveKeyMapFile (const std::string& path, const KeyMap& k)   { return vt::writeFileBytes (path, vt::write (keyMapToTrees (k))); }
}
