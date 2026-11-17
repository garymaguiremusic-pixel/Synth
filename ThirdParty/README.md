# ThirdParty

Copies of Gary's own plugins, used unmodified as VC-Clone's global sections:

| Folder | Source repo | Used for |
|---|---|---|
| `VirusDelay/` | garymaguiremusic-pixel/Virus-Delay | global Delay (processor + panel) |
| `LeadVerb/`   | garymaguiremusic-pixel/LeadVerb    | global Reverb (processor + panel) |
| `ArpSampler/` | garymaguiremusic-pixel/ArpSampler (`Arp/`, `Parameters.h`, `UI/StepGrid`, `UI/Shared`) | global Step Sequencer (engine + step grid, no sampler) |

Each copy keeps its own class names. Names that clash between the plugins (`CustomLookAndFeel`, `VirusKnob`, ...)
are renamed at compile time with per-file macros (see `cmake/vendor.cmake` and `Plugin/Vendor*.h`), so the files can
be refreshed from their repos without edits. `createPluginFilter` is renamed the same way.
