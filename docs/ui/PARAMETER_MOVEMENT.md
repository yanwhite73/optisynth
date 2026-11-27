# Movement on a parameter

Requests 94–95. Interaction proposal for the desktop synth. The first review should use Modal Percussion Decay and Analog VCO pulse width at the smallest supported rack zoom.

## Checking that movement can be heard (0.0.37, request 146)

A route can be correct and still inaudible. Possible causes:
- the destination barely changes the sound;
- another layer masks it;
- the base sits at the end of the range, which clips the sweep.

Run `Optisynth --movement-audit "<preset or factory:N>" "<folder>"`, then `python3 tools/movement_audit.py "<folder>"`. For each route, the report gives the change in dB between the two ends of the sweep and between the ends of the whole range. It also compares the patch with and without its movement, half a second at a time.

The audit opens no audio device and never writes the preset. Add `node.parameter=value` to try a different setting, and `seconds=N` to change the length of the moving render.

## Reading movement in 0.0.34

The coloured arc shows the sweep and its white marker follows the parameter target. The static number beside the arc opens that LFO's editor. A plus after the number means the knob has additional sources, all listed in the editor. When several sources act together the white outer arc covers their combined possible range. An unassigned knob has a small wave button to add movement. Opening, assigning, layering and removing movement while a sequence plays preserve its position and sounding notes.

Open the wave button to see the current destination value, the sweep endpoints in its own units, and the running frequency of each LFO. Colour Rate in the Prism Mallet panel is itself an LFO frequency. Assigning movement there can vary another control's cycle length. A source with no path to an audio output is identified in the editor. For an edited patch, use the source numbers on its own panel to identify assignments.

Depth is signed. On Hardness, whose full range is four octaves, 10% means a swing of 0.4 octaves either side of the base. On a logarithmic control such as Colour Rate, the percentage is a fraction of its logarithmic span. Negative values reverse polarity. Type a rate as Hz or a duration such as `4 s` or `500 ms`. New assignments on controls inside their range use at most the headroom available on the nearer side.

## Using movement in 0.0.32

Click the wave button beside a continuous knob in Panel, Modular or Focus.
The first click on an unassigned knob creates a free-running sine LFO with
a four-second period. The editor opens beside the knob. Rate shows both
frequency and period, for example **0.380 Hz / 2.6 s**.

In **Prism afterimage → Mallet**, open **Brightness**. Depth changes the sweep
on Brightness alone. Rate and Shape also affect Strike because both use the
same LFO. Choose **Make independent** to give Brightness its own rate. Use
**Bypass** on a matrix assignment to audition its contribution or **+ Add
movement** to layer another source. Older direct CV cables expose the source
level. Each LFO keeps its colour across destinations; the arc shows the
extent and the white dot follows the modulation during playback.
[Brightness movement editor](module-panels/movement-editor.png)

## The first five seconds

A pattern is already playing. The player hears the same percussion tail on every strike and wants its length to breathe. Beside **Decay**, click a small wave-shaped button. The next strikes get alternately shorter and longer. The panel stays on the percussion voice. A small anchored editor shows **Decay movement**, **Depth** and **Rate**. The original knob still sets the centre. Bypass the movement to hear the original pattern, then restore it at the same settings. Dismiss the editor and keep playing.

The action should also make sense with eyes closed. On keyboard focus the button announces “Add movement to Decay”. Enter assigns and opens the editor. Its next focus target is Depth, followed by Rate. Escape returns focus to that button. On an existing assignment its name becomes “Edit Decay movement”. The accessible readout gives base, extent and source; an animated knob position alone cannot communicate those distinctions.

## Two placements worth testing

| Placement | What the player sees and does | What to watch in the mock-up |
| --- | --- | --- |
| A. Small waveform button aligned beside the parameter label | The knob keeps its present drag target. A single click applies movement and opens the local editor. A thin, static extent arc appears around the knob. | Can someone find the affordance without being shown? Does the label still fit on Pitch, Pulse Width and PWM Depth? |
| B. Draggable outer ring | Drag the ring away from zero to create movement and set its depth in one gesture. A click on the ring gives the same editor. | At minimum zoom, does a player aiming for the knob change depth, or vice versa? Can a trackpad user acquire the ring without precision pointing? |

Start with **A**. Its separate hit target follows the modal controls people already use and it leaves the centre knob predictable. Keep the secondary target the same usable size across the rack and focus editor. Collapse verbose text before reducing its target. The project’s Pigments reference offers a useful convention for seeing modulation extent and routes; creation here begins at the destination while the player's attention is on its sound. [Existing panel and matrix study](CONTEXTUAL_UI.md)

## Proposed behavior

| Situation | Visible behavior and effect |
| --- | --- |
| Available and unassigned | Quiet outlined wave, always discoverable. Click once for a private, free-running sine LFO with modest audible depth. Choose starting depth by destination and show the range in the destination's units. Prototype a four-second period for Decay. A delay-time deviation should start much smaller than a decay sweep. |
| Movement assigned | Filled wave and static extent arc. The knob marker stays at the saved base. Show a moving tick for the effective value only while inspecting the parameter. Repeated clicks reopen the assignment. A display such as “400 ms base · 310–515 ms” can express a logarithmic range. |
| Adjust while playing | The base knob edits the centre. Depth edits only this destination's signed amount. Rate has seconds/Hz and, when supported, a beat division. Shape and retrigger live in a second row. Scrolling over any of these controls navigates the rack. Support typed values, tab and arrow keys. |
| Bypass, remove or undo | Bypass remembers rate, shape and depth for a musical A/B. Remove deletes only this assignment. One undo of first assignment restores the exact preceding patch. A private source can be disposed of when its last use disappears. |
| Another source already reaches the parameter | Show the existing source names and their amounts in the local editor, with an explicit Add movement action. Preserve every existing cable and matrix row. Reopening a legacy modal LFO allows editing the existing source. A new route must never silently double the depth. |
| Player wants coordinated motion | Offer “Use existing LFO” in the editor. A shared source is labelled with its destination count. Rate and shape affect that whole group; depth, polarity, bypass and removal belong to this assignment. “Make independent” copies the source settings for this destination. |
| The control is waiting for a condition | Preserve the hollow activity lamp and state the reason in words, e.g. “FM Depth needs a signal at FM”. A saved assignment can wait and resume when the patch makes the knob effective. A distinct wave glyph identifies assignment even when the graph is quiet. |
| The control chooses a structural mode or loads a resource | Explain why continuous movement is unavailable. For discrete *musical* parameters, design explicit stepped behavior individually. Continuous interpolation through filenames, module types or graph topology has no useful meaning. |

At a parameter boundary, show the range the engine can actually reach. A clipped sweep has a different feel from one centred well inside the range. A private LFO should initially run across successive strikes; restarting it at the same phase on every trigger could make a slow decay movement inaudible. Keep reset policy visible. A rack-level source moves all affected voices together. Independent per-voice retrigger requires an explicitly supported voice-level source.

## Routing and cost

When this study was written, the modal shortcut created a visible 1 Hz, depth 0.35 LFO, cabled it to a typed CV input and selected its source module. The matrix already saves instance-qualified destinations with signed, normalised depth. It adds contributions, clamps the result and supports log-scaled targets. The two paths are distinct today. [Panel control](../../app/Panel.h) · [Modal assignment](../../app/RackEditor.h) · [Matrix editor](../../app/ModulationEditor.h) · [Runtime contract](../architecture/SPECTRAL_ENGINE_INTEGRATION.md)

For new assignments, prefer one persisted source and one matrix route with its own amount. Keep the source at unit amplitude so the local Depth has one unambiguous meaning. Signal Flow and the matrix should identify the same assignment. Explicitly mark private ownership for recall, duplication and deletion. Rack disclosure should reveal the source on demand without inserting a big card beside every knob. Existing CV patches retain their exact scaling; verify old preset round-trips before considering migration. A cable with no route attenuator should expose source editing, with shared scope identified. UI for local bypass/depth on that cable needs a defined DSP path.

An idle affordance costs no oscillator. Prepare a source only for an assigned, reachable destination and reuse one source for linked targets. Source creation and structural changes use the graph preparation path. Real-time depth changes use the existing route mechanism. Specify smooth application and verify sustained notes and delay tails survive adding movement during playback. Surface a rejected feedback cycle or unreachable target at the control that initiated the action.

## Usability review before implementation

1. At minimum and normal rack size, hand a player a running modal patch and ask them to make the decay breathe. Observe the first click, time to audible movement, stray base-value edits and whether they retain their place. Compare A and B on the same task.
2. On a sustained pulse, adjust the base width and then only its sweep. Ask the player to identify the base and endpoints, bypass the movement, and restore it. Test keyboard-only operation, trackpad scrolling and the reduced-motion presentation.
3. Open a preset where one LFO drives two destinations. Reduce the motion of only the second, then change only its speed. Verify they can discover which control is shared before accidentally changing the first sound.
4. Reopen an old cabled modal preset and a patch containing a waiting FM Depth. Ask what is assigned and what is currently audible. The player should be able to explain both, inspect routing and remove one assignment without disturbing another.
5. For the musical trial, keep a pitched sequence and echo time fixed. Move percussion decay, then move only delay time on a separate pass. Confirm that a listener can hear the first as changing ring length and the second as changing the returning sound, and can recover the dry reference instantly.

The first decision after comparing the mock-ups is placement and target size. Next resolve which destinations warrant a special depth range, which support beat sync, and which historical CV routes can offer genuinely local editing. These are open design and listening questions.

[Artist studies and patch experiments](../research/instruments/RICH_ROACH_PATCH_STRATEGIES.md)
