# Licensing policy and reference instruments

Request 50, 19 November 2026: Optisynth is ultimately a **commercial, closed-source
product**. Other projects may be studied to learn from; only licence-compatible
code may be adopted.

## Rules

- **Adopt (with notices kept):** MIT, BSD, Apache-2.0, Boost, zlib, public domain.
  Review the file and its dependencies, pin the revision and record provenance,
  as for Mutable.
- **Study only:** GPL, AGPL, LGPL and CC-BY-SA projects. Read them for technique
  and behaviour, describe what we learn in our own words with a link, and
  implement from papers, datasheets, schematics and our own measurements. Never
  copy their code, coefficient tables or presets.
- **Commercial plugins:** listening and A/B references only. Do not reverse
  engineer their binaries. Their presets are not ours to ship.

## Current dependencies

| Component | Licence | Commercial status |
| --- | --- | --- |
| JUCE 8.0.8 (`~/JUCE`) | AGPLv3 or the commercial JUCE 8 licence | **Buy a JUCE licence before selling.** AGPL would force open source |
| VST3 SDK | MIT since 3.8 (October 2025) | OK. The VST name and logo follow Steinberg's trademark rules |
| Mutable `plaits`/`stmlib` (vendored) | MIT | Code OK. **Names are not:** upstream says "Mutable Instruments" must not appear on derivative works and advises against original module names. Before release, rename user-facing text: "Original Plaits DSP", "Original Mutable stmlib DSP" and the saved IDs `mutable.*` |
| MAME CEM chip models (candidate) | BSD-3-Clause per file | OK with notices; check each dependency's header |
| Inherited Spectral engine | First-party code | Modelled on LinPlug's product, with `.ptch` import. **Review the "Spectral" name and preset import before release** |

## Research candidates reclassified

- **Adoptable after review:** Airwindows (MIT), Clouds/Parasites (MIT), Marbles
  (MIT), PER\|FORMER (MIT), 4ms DLD software (MIT), CHOW Matrix (BSD-3; check its
  dependencies), MAME chip models (BSD-3).
- **Study only:**
  - Valley Plateau, Grids, OB-Xf, Odin 2, Surge XT, Vital, Cardinal and GliGli
    p600fw (all GPL-3).
  - DaisySP ReverbSc (LGPL).
  - Shruthi 4PM board (CC-BY-SA).

## Installed reference instruments

Read-only scan of `/Library/Audio/Plug-Ins` and `/Library/Arturia` on 19 November.
Only bundle metadata was read; nothing was loaded. Pad counts are factory
presets with pad, swell, string, ambient or drone in the name.

| Plugin | Version | Relevance | Factory presets (pad-like) |
| --- | --- | --- | --- |
| Arturia Matrix-12 V2 | 2.13.4 | Xpander / Matrix-12 (Roach) | 456 (47) |
| Arturia SEM V2 | 2.13.2 | SEM / Oberheim filter | 574 (46) |
| Arturia Prophet V3 | 3.9.1 | Prophet-5 (Rich) and Prophet-VS | 712 (67) |
| Arturia CS-80 V3, Jup-8 V3, Solina V2 | 3.10.1, 3.6.3, 2.13.4 | Classic pad and ensemble references | 566 (50), 507 (47), 148 (28) |
| Arturia Modular V3, Synthi V, Buchla Easel V | 3.13.4, 1.9.4, 1.13.4 | Berlin School modular sequencing and textures | 537 (45 pad, 40 seq/arp) |
| Valhalla Supermassive, VintageVerb, SpaceModulator, FreqEcho | 4.0.0, 4.0.5, 1.2.8, 1.2.8 | Ambient reverb and delay benchmarks | — |
| Surge XT, Vital, Cardinal | 1.3.4, 1.5.5, — | Open-source references (GPL: study only) | — |
| Airwindows Consolidated | 1.2025.63 | MIT: its source is adoptable | — |

Arturia's OP-Xa V (the OB-Xa emulation) is not installed. No Arturia manual PDFs
are installed locally; the manuals are online.

**How to use them:**
- **Arturia factory presets** are plain-text Boost archives. The randomizer
  project already parses this container
  (`../../../ableton/randomizer/docs/pigments_preset_format.md`). Mining the pad
  presets for envelope times, filter settings, detune and modulation depths
  would tell us numerically what "lush" means. Check Arturia's EULA first, use
  only derived statistics, and never ship the presets themselves.
- **Offline A/B renders** mean hosting these plugins in a test tool. Under the
  workspace third-party rule, read each plugin's documentation first, and ask
  Yan before any plugin is loaded.
