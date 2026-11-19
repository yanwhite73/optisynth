# Oberheim and Prophet: circuit documentation, plugins and open source

Request 47, 19 November 2026. This is the first input to the analog pad voice
proposed in `../../project/SOUND_GOALS.md`. It surveys what circuit documentation,
commercial plugins and open-source code exist for the reference instruments:
Oberheim SEM, OB-X/OB-Xa/OB-8, Xpander/Matrix-12, and Prophet-5 (plus the
Prophet-6 that Rich cites).

**How far each item was checked.** Rows marked "located" were found through
search results or listings; the document itself was not read. Rows marked "read"
quote the source. For MAME, the file headers and component usage were read
through the GitHub API at the revision current on 19 November 2026 (latest
Xpander voice-board change 27 October 2026). The full model source has not been
reviewed. Nothing was downloaded, built or run for this survey.

## Short answer

- **Circuit documentation is plentiful for the vintage instruments.** Service
  manuals and schematics exist for the SEM, OB-X, OB-Xa, OB-8, Xpander,
  Matrix-12 and every Prophet-5 revision. Datasheets exist for the Curtis
  (CEM) and SSM chips they use. We already hold the Xpander owner manual,
  service manual and schematic archive locally; they have not been analysed yet.
- **Current products are not documented.** I found no public schematics for the
  Prophet-6, Prophet-5 Rev4 or OB-X8, and did not search specifically for them.
- **Several commercial plugins model these instruments,** some officially licensed.
- **Open source is the important finding.** MAME has component-level models of
  the actual Curtis chips under the permissive BSD-3-Clause licence. It also has
  a Prophet-5 Rev 3.0 driver that uses them, and an Xpander voice board under
  active development. The full open-source polysynths (OB-Xf, Odin 2) are GPL-3.

## Circuit documentation

| Instrument | What exists | Key circuit facts | Checked |
| --- | --- | --- | --- |
| Xpander | Owner manual, 39-page service manual, schematic ZIP | Six voices. Each voice is a CEM3374 dual VCO plus a CEM3372 VCF/VCA. A 4051 multiplexer selects one of 15 filter modes. VCO2 can modulate VCO1 or the VCF. Shared noise source. LFOs and envelopes are digital and reach the voices as control voltages through a 14-bit DAC. | Local copies; header read in MAME [`xpander_vb.cpp`](https://github.com/mamedev/mame/blob/master/src/mame/oberheim/xpander_vb.cpp) |
| Matrix-12 | Service manual and parts list | Two Xpander voice boards plus a keyboard; the other boards are "identical or nearly identical" | Read in MAME [`xpander.cpp`](https://github.com/mamedev/mame/blob/master/src/mame/oberheim/xpander.cpp); [parts list](https://manualmachine.com/oberheim/matrix12/6489851-parts-list) located |
| Xpander filter | Third-party circuit analyses | Pole mixing: buffered taps on each CEM3372 filter-stage capacitor are combined with weighted sums. The Shruthi "4-pole Mission" board uses the same technique. | [Electric Druid](https://electricdruid.net/multimode-filters-part-2-pole-mixing-filters/) and [CEM3372](https://electricdruid.net/product/cem3372-synth-voicing-chip/) pages located (search summary) |
| OB-8 | 34-page service manual (January 1982) with schematics | CEM3340 VCOs, CEM3310 envelopes, CEM3320 filter switched between a 4-pole cascade and a 2-pole state-variable topology | Mirrors: [emusic-diy Oberheim manuals](https://emusic-diy.org/oberheimmanuals) (links Magnus Danielson's page), [lost-manuals](https://www.lost-manuals.com/equipment.php?id=16295); chip facts from [Electric Druid](https://electricdruid.net/?p=1807) and the [chips list](https://www.sequencer.de/synth/index.php/Chips_in_Synthesizers) |
| OB-Xa | Service manual | CEM3320 used for both 2-pole and 4-pole filters; CEM3340 VCOs | [soundprogramming.net](https://soundprogramming.net/synthesizers/oberheim/oberheim-ob-xa) and manualslib listings located |
| OB-X, SEM | OB-X service manual; SEM and Mini-Sequencer schematics | SEM: discrete 12 dB/oct multimode state-variable filter | [emusic-diy Oberheim manuals](https://emusic-diy.org/oberheimmanuals) located |
| Prophet-5 Rev 1 | Service manual | SSM2030 VCO, SSM2040 VCF, SSM2050 envelope, SSM2020 VCA | [sdiy.info Prophet-5](https://sdiy.info/wiki/Sequential_Circuits_Prophet-5?oldid=8967) located |
| Prophet-5 Rev 2/3 | Technical manual with Rev 2 and Rev 3.2 schematics; Rev 3.0–3.2 conversion notes | Rev 3 is a redesign: CEM3340 VCO, CEM3320 VCF, CEM3310 envelope, CA3280 VCA. Rev 2 (serials 184–1299) is usually considered the better-sounding version. | [audiofanzine service-manual text](https://medias.audiofanzine.com/files/sequentialcircuitsprophet-5servicemanual-text-470674.pdf) and sdiy.info located |
| Prophet-6, Prophet-5 Rev4, OB-X8 | None found | Current products | Not specifically searched |

**Datasheets.** CEM3310, CEM3320, CEM3340, CEM3372 and SSM2040 datasheets are
widely mirrored, as are reissue parts such as Alfa AS33xx and SSI2140. MAME's
models cite datasheet equations and component values. Individual datasheet URLs
are not yet catalogued here.

**What the schematics cannot give us on their own:** nonlinear behaviour,
component tolerances, voice-to-voice variation, and calibration or trimmer
settings. These need measurements or careful modelling (`../RESEARCH.md`
already makes this point for the Xpander).

## Commercial plugins (listening and A/B references)

| Plugin | Models | Notes |
| --- | --- | --- |
| Arturia Matrix-12 V2 | Matrix-12, which effectively covers the Xpander | 15-mode filter; matrix extended to 27 x 47 ([V Collection](https://arturia.com/products/analog-classics/v-collection/details)) |
| Arturia OP-Xa V | OB-Xa | Curtis 2- and 4-pole filters; no SEM filter ([review](https://www.pressreader.com/australia/future-music-9629/20200728/283596697606740)) |
| Arturia SEM V3 | SEM (expanded to 8 voices) | Also the Filter SEM and Filter M12 effect plugins |
| GForce OB-E v2 / OB-EZ | Oberheim 8-Voice (eight SEMs) | Endorsed by Tom Oberheim, built with ex-Oberheim engineer Marcus Ryle ([MusicRadar](https://www.musicradar.com/news/gforce-software-obe-2-synth-plugin-tom-oberheim)) |
| GForce OB-X | OB-X | Endorsed by Tom Oberheim ([Synth Anatomy](https://synthanatomy.com/2023/10/gforce-software-oberheim-ob-x-first-look-tom-s-first-ob-analog-polysynth-as-a-plugin.html)) |
| Cherry Audio Eight-Voice | Oberheim 8-Voice | Low-cost ([MusicTech](https://www.musictech.com/news/gear/cherry-audio-eight-voice/)) |
| u-he Repro-5 | Prophet-5, component-level | Reviewers cite the CEM3320 filter, which implies Rev 3; u-he's stated revision not confirmed ([Attack review](https://www.attackmagazine.com/reviews/gear-software/attention-to-detail-u-he-repro-5/)) |
| Arturia Prophet-5 V | Prophet-5 (bundled with Prophet-VS) | |
| GForce Prophet-5 | Official Sequential-licensed emulation | Reported with MPE and dual layers ([report](https://feedbagel.com/post/sequential-prophet-5-gets-an-official-software-remake-by-gforce-tested-cdm-creat)) |
| u-he Diva | Several analog filters, including "Uhbie" | Whether Uhbie models the SEM is not confirmed by the sources checked |
| Autodafe Vintage Emulator Studio | Wraps MAME synth cores, including Prophet-5 | Free ([Tom's Hardware](https://www.tomshardware.com/video-games/retro-gaming/vintage-emulator-studio-recreates-44-legendary-synths-down-to-the-chip-level-free-mame-powered-component-level-emulation-should-offer-exceedingly-accurate-sound)) |

There is no OB-X8 plugin, and no dedicated Xpander plugin apart from Arturia's
Matrix-12 V. Plugins are behaviour and listening references only; their code is
closed.

## Open source

| Project | Licence | Relevance | State |
| --- | --- | --- | --- |
| [MAME](https://github.com/mamedev/mame) chip models: `cem3310`, `cem3320`, `cem3340`, `cem3372`, `cem3360`, `va_vcf`, `va_vco`, `va_eg`, `va_vca` | BSD-3-Clause in each file header (mainly m1macrophage; `va_vcf` with Olivier Galibert) | Component-level models of the exact chips: a 4-pole low-pass with resonance and drive, a VCO with datasheet waveform levels, envelopes and VCAs | Active. The CEM3320 drive value was "settled ... after experimentation", and the header says the correct value needs real-device measurement |
| MAME `prophet5.cpp` | BSD-3-Clause | Prophet-5 Rev 3.0 driver (3,564 lines) built from the technical manual, using the CEM3340/3320/3310 models; firmware, autotune and CV multiplexing | Emulates the instrument. The voice-architecture outline is still marked TODO |
| MAME `xpander_vb.cpp` | BSD-3-Clause | Xpander voice board: CEM3372 filter and pole mixer for the 15 modes | Partial. Filter and pole mixer are implemented; "TODO: Add oscillators 1 and 2". `xpander.cpp` says "There is no attempt to emulate audio", which may now be out of date |
| MAME `ob8.cpp`, `matrix6.cpp`, `prophet600.cpp` | BSD-3-Clause | OB-8, Matrix-6 (CEM3396) and Prophet-600 control-side drivers | OB-8 is "an early-stage skeleton"; no audio |
| [OB-Xf](https://synthanatomy.com/2025/09/surge-synth-team-ob-xf.html) (Surge Synth Team) | GPL-3.0 | Complete OB-X-style polysynth (AU/VST3/CLAP/LV2), continuing 2DaT's open OB-Xd | Beta, 1.0 expected; repository active (pushed 17 Nov 2026). Separate from discoDSP's closed-source OB-Xd 3 |
| [Odin 2](https://github.com/TheWaveWarden/odin2) | GPL-3.0 per the developer and KVR (GitHub does not auto-detect the licence file) | SEM-style 12 dB state-variable, ladder, Korg-35 and diode-ladder filter emulations | Maintained (UI overhaul in 2025) |
| [GliGli p600fw](https://github.com/gligli/p600fw) and forks | GPL-3.0 per the project page; some files carry their own headers | Replacement Prophet-600 firmware: envelopes, LFO, voice assignment and unison logic. Behringer's PRO-800 reportedly builds on it | Control code, not audio DSP |
| Mutable Shruthi [4-pole Mission board](https://pichenettes.github.io/mutable-instruments-diy-archive/shruthi/4pm/) | CC-BY-SA-3.0 (circuit and PCB) | Uses "the same 'Pole-mixing' technique introduced in the Oberheim Xpander". 15 modes: lp4, lp3, lp2, lp1, hp1, hp2, hp3, bp2, bp4, notch, phaser, h2lp, h3lp, n+lp, p+lp. SSM2164 core with an LM13700 VCA | Read. Discontinued kit; design archived. A ready, readable mode table to compare with the Xpander manual |
| Zavalishin, *The Art of VA Filter Design*, rev 2.1.2 ([author's KVR thread with links](https://www.kvraudio.com/forum/viewtopic.php?p=5990215)) | Free PDF; may be copied freely in full. The former Native Instruments URLs now return 404 | The standard reference for zero-delay-feedback state-variable, ladder and OTA filters | Book, not code |
| Lazzarini and Timoney, [Chamberlin SVF paper](https://arxiv.org/pdf/2111.05592.pdf) | arXiv | Digital state-variable analysis; cites the OB-Xa CEM3320 2-pole circuit | Paper |

## Licence consequences

- **Usable with any Optisynth licence:** MAME's BSD-3-Clause files, Mutable's
  MIT code and our own implementations. BSD-3 requires keeping the copyright
  notice. Each file's dependencies (`va_ops`, `flt_rc`, `rescap`, the device and
  stream framework) need their own headers checked.
- **Copyleft:** adopting OB-Xf, Odin 2 or p600fw code would make Optisynth
  GPL-3. Optisynth's own distribution licence (including how JUCE is licensed)
  has not been decided. Until it is, treat GPL projects as design and listening
  references only.
- **MAME adaptation effort:** its models depend on MAME's device and stream
  framework. Adapting them is a port, similar in scale to the Mutable
  adaptation, not a copy-and-paste.

## Recommendation

1. **Prophet-style voice:** use MAME's BSD-3 CEM3340/3320/3310 models and the
   Prophet-5 Rev 3 driver as the primary reference, checked against the Rev 3
   technical manual. The filter can be implemented as a zero-delay-feedback
   4-pole OTA cascade (Zavalishin). Rev 2's SSM2040 is the alternative if its
   sound is preferred; it has no MAME model.
2. **Oberheim-style voice:** the Xpander route is the best documented. We hold
   the service manual and schematics, and MAME's CEM3372 and pole-mixer code
   covers the 15 filter modes. For OB-8 pads, the CEM3320 switched between
   4-pole and 2-pole state-variable modes reuses the same chip model.
3. **SEM filter:** write our own from the SEM schematics and Zavalishin's
   state-variable treatment. Odin 2's SEM filter serves as a GPL listening
   reference.
4. **References for measuring our results:** commercial plugins Yan owns,
   rendered offline with matched settings. Ask which ones he has.
5. **Before any code is adopted:** review each MAME file and its dependencies
   in full, pin a revision and record provenance, following the Mutable
   procedure.

## Next research steps

- Download and hash the Prophet-5 Rev 3 technical manual, the OB-8 service
  manual, the SEM schematics and the CEM3310/3320/3340/3372 datasheets into
  `../../references/`, then read the voice sections.
- Analyse the local Xpander service manual and schematics: the voice card, the
  mode multiplexer table and the pole-mixing resistor values.
- Read the full source of MAME's `cem3320`, `cem3372`, `va_vcf` and the Prophet-5
  voice path. Record the equations, the assumptions and every value marked as
  experimental.
