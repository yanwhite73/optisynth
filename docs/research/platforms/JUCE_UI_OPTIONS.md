# Existing UI frameworks and code for Optisynth (request 72)

20 November 2026. Yan: "isnt there a Juce UI toolkit?" and "I have to continually
prod and poke to remind you agents to use existing frameworks and open source code".
This survey should have preceded request 71. Licences are judged against
[LICENSING.md](../../project/LICENSING.md).

| Option | What it gives | Licence | Decision |
| --- | --- | --- | --- |
| JUCE `Grid`, `FlexBox` (juce_gui_basics) | CSS-style grid and flexbox layout | JUCE licence (already a dependency) | **Used** for module cards, rack rows and synth-panel sections |
| JUCE `TableListBox`, `ListBox`, `PropertiesFile`, `LookAndFeel_V4` | Sortable tables, lists, settings files, styling | JUCE | **Used** in the preset browser, settings and value-text styling |
| [JIVE](https://github.com/ImJimmi/JIVE) | Declarative JUCE GUIs from markup, flex/grid layout, style sheets | MIT | Candidate for a later move to declarative layouts; its docs and source must be read first |
| [melatonin_inspector](https://github.com/sudara/melatonin_inspector) | Browser-style live inspector for JUCE component bounds | MIT | Candidate dev-only tool for layout work |
| [chowdsp_utils](https://github.com/Chowdhury-DSP/chowdsp_utils): `chowdsp_presets_v2`, `chowdsp_fuzzy_search` | Preset management; fuzzy search | BSD (per module README) | Presets v2 assumes plugin-parameter state, which our graph documents are not. Fuzzy search could improve the browser search box |
| chowdsp `chowdsp_gui` (preset component) | Preset UI | GPLv3 | Study only |
| [foleys_gui_magic](https://github.com/ffAudio/foleys_gui_magic) | WYSIWYG GUI editor, FlexBox-based | Licence unclear (sources disagree) | Not used until the LICENSE file is read |
| HISE preset browser (tags) | Tagged preset browser | GPLv3 | Study only |

Sources: the JUCE module headers in `~/JUCE/modules/juce_gui_basics/layout/`;
the [awesome-juce list](https://github.com/sudara/awesome-juce); the
[chowdsp_utils README](https://github.com/Chowdhury-DSP/chowdsp_utils).

**Rule adopted.** Every new UI or infrastructure feature starts with a short note
like this: built-in, open-source, licence and fit. Hand-written code is only for
what nothing suitable covers.
