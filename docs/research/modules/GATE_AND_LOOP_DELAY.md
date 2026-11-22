# Optomix and Dual Looping Delay in the Optisynth rack

The 0.0.19 modules are independent DSP implementations of selected panel
behaviours. Their gain curves, timing limits and controls below describe
Optisynth. Hardware audio comparisons remain open.

## Dual low-pass gate

The Optomix Rev 2 manual, pp. 5–8, describes two gates that shape level and
frequency content together. Each has Control, Strike, Damp and its own output.
The auxiliary input joins the two outputs at Sum. Strike opens a gate with a
pulse, and increasing damping shortens the response. [Make Noise manual](https://www.makenoisemusic.com/wp-content/uploads/2024/03/optomixrev2manual.pdf)

The rack module exposes both audio inputs, separate strike, control and damp
sockets per channel, both individual outputs, Aux and Sum. Control is an
offset added to the control input. Damp CV adds to manual damping. Optical
opening follows excitation quickly and falls more slowly. The same opening
sets gain and a one-pole filter cutoff. Key strike sets velocity sensitivity
per channel and can be turned down for exclusively patched triggering.

**Reed squelch** routes a swept resonant saw to gate A and a lower sine to B.
Play repeated low notes. Increase B Damp to keep the sub as an attack and use A
Damp to let the reed ring longer. The separate resonant filter supplies the
vowel sound. The LPG supplies its changing brightness and decay. Optomix
side-chain compression, voltage normalisations and measured vactrol matching
remain outside this version.

## Dual looping delay

The DLD v5 manual, pp. 1–4, describes two channels sharing a time base, each with
its own loop length, input feed, feedback, wet balance, infinite hold and reverse.
Hold suspends recording while a captured passage repeats. The individual delay
outputs can feed other modules. [4ms manual](https://4mscompany.com/DLD/manual/DLD-manual-1.1-v5.pdf)

Both lines follow the rack tempo. Two rising edges on Ping between 20 and
300 BPM establish an external beat. The most recent external period is retained
until the graph is rebuilt. Stereo input feeds A/B and the
two additional mono inputs allow separate sources. The main output is stereo.
Wet A and Wet B are independent mono taps. Time is measured in fractions or
multiples of a quarter note, up to eight beats. Each channel has 24 seconds
of memory at the rack's internal 48 kHz. Feedback and feed each range from
zero to unity. Hold captures the current length and preserves it across tempo
changes until released. Reverse changes read/write direction during recording
and playback direction during Hold. Hold and Reverse can be set per channel
on the panel or via the modulation matrix. Loop recordings live in memory.
Presets save the routing and controls.

**Glass loops** sends a struck Mutable modal voice into both channels at
different divisions. Play three high notes, hold A, and reverse A while playing
new notes into B. **String afterglow** sends the polyphonic swell and its stereo
effects to a pair of slower echoes. Hold one side as the chord changes.

Clock outputs, external feedback inserts, movable loop windows,
over-unity regeneration and the hardware's nearly three-minute memory per
channel remain possible extensions. The module uses only the existing rack audio,
note and clock interfaces. It opens no ports and loads no external firmware.
