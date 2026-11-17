#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace optisynth {

enum ClockChange : unsigned { ClockNone=0, ClockStart=1, ClockStop=2, ClockSeek=4, ClockLoop=8 };
struct ClockFrame {
    uint64_t sample = 0, revision = 0;
    double beat = 0, tempo = 120, barStart = 0, beatsPerBar = 4;
    unsigned changes = ClockNone;
    bool playing = false;
};

// Audio-thread-owned local transport. Commands must be delivered by the owner;
// this is deliberately not a cross-thread mailbox or a host transport adapter.
class SampleClock {
public:
    explicit SampleClock(double sampleRate=48000);
    bool tempo(double bpm) noexcept;
    bool meter(int numerator,int denominator) noexcept;
    bool seek(double quarterNotes) noexcept;
    bool loop(bool enabled,double start=0,double end=4) noexcept;
    void play(bool running) noexcept;
    ClockFrame next() noexcept;
private:
    double position() const noexcept;
    void anchor(double beat) noexcept;
    void changed(unsigned reason) noexcept;
    double rate_, bpm_=120, anchorBeat_=0, barLength_=4;
    double loopStart_=0, loopEnd_=4;
    uint64_t sample_=0, anchorSample_=0, revision_=0;
    unsigned changes_=ClockNone;
    bool playing_=false, looping_=false;
};

// A division of the shared clock, not another time accumulator. Reset is
// reported separately from an optional grid boundary at the same sample.
struct GridPulse { bool reset=false, tick=false; int64_t index=0; };
class GridClock {
public:
    bool division(double quarterNotes) noexcept;
    GridPulse consume(const ClockFrame&) noexcept;
private:
    double division_=.25;
    uint64_t revision_=0;
    int64_t index_=0;
    bool seen_=false;
};

constexpr uint32_t allNoteSources=std::numeric_limits<uint32_t>::max();
constexpr size_t maximumNoteEvents=256, maximumHeldNotes=64;
constexpr uint32_t maximumEventBlockFrames=65536;
enum class NoteEventKind : uint8_t { Reset, Off, On };
struct NoteEvent {
    uint32_t offset=0, source=0;
    uint64_t noteId=0;
    NoteEventKind kind=NoteEventKind::Off;
    int pitch=60;
    float velocity=0;
};

// Resolved IDs and sample offsets only. Failed blocks contain no events and
// require release of the affected bus. The failure stays latched until begin().
class NoteEventBlock {
public:
    void begin(uint32_t frames) noexcept;
    bool push(const NoteEvent&) noexcept;
    void fail() noexcept { failed_=true; size_=0; }
    bool failed() const noexcept { return failed_; }
    size_t size() const noexcept { return size_; }
    const NoteEvent* begin() const noexcept { return events_.data(); }
    const NoteEvent* end() const noexcept { return events_.data()+size_; }
private:
    std::array<NoteEvent,maximumNoteEvents> events_{};
    uint32_t frames_=0;
    size_t size_=0;
    bool failed_=false;
};

struct HeldNote {
    uint32_t source=0;
    uint64_t noteId=0, order=0;
    int pitch=60;
    float velocity=0;
    bool active=false;
};
enum class NoteChange { Ignored, Added, Updated, Released, Reset, Overflow };
// Ownership ledger for future arps/event adapters. Not an audio voice allocator.
// Call beginBlock before consuming events; overflow clears all held identities.
class HeldNotes {
public:
    void beginBlock(const NoteEventBlock& block) noexcept { muted_=block.failed();if(muted_)clear(); }
    NoteChange apply(const NoteEvent&) noexcept;
    void clear(uint32_t source=allNoteSources) noexcept;
    size_t size() const noexcept { return size_; }
    const HeldNote* newest() const noexcept;
    const std::array<HeldNote,maximumHeldNotes>& notes() const noexcept { return notes_; }
private:
    std::array<HeldNote,maximumHeldNotes> notes_{};
    size_t size_=0;
    uint64_t order_=0;
    bool muted_=false;
};

} // namespace optisynth
