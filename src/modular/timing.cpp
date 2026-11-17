#include "timing.h"
#include <cmath>
#include <stdexcept>

namespace optisynth {
namespace {
constexpr double maximumBeat=1e9;
bool validBeat(double beat) noexcept { return std::isfinite(beat)&&std::abs(beat)<=maximumBeat; }
bool validEvent(const NoteEvent& e) noexcept {
    if(e.kind==NoteEventKind::Reset)return true;
    if(e.source==allNoteSources||e.noteId==0)return false;
    if(e.kind==NoteEventKind::Off)return true;
    return e.kind==NoteEventKind::On&&e.pitch>=0&&e.pitch<=127&&
        std::isfinite(e.velocity)&&e.velocity>0&&e.velocity<=1;
}
bool before(const NoteEvent& a,const NoteEvent& b) noexcept {
    if(a.offset!=b.offset)return a.offset<b.offset;
    if(a.kind!=b.kind)return a.kind<b.kind;
    if(a.source!=b.source)return a.source<b.source;
    return a.noteId<b.noteId;
}
}

SampleClock::SampleClock(double rate):rate_(rate) {
    if(!std::isfinite(rate)||rate<8000||rate>384000)throw std::invalid_argument("Unsupported clock sample rate");
}
double SampleClock::position() const noexcept {
    return anchorBeat_+(playing_?double(sample_-anchorSample_)*bpm_/(60*rate_):0);
}
void SampleClock::anchor(double beat) noexcept { anchorBeat_=beat;anchorSample_=sample_; }
void SampleClock::changed(unsigned reason) noexcept { changes_|=reason;++revision_; }
bool SampleClock::tempo(double bpm) noexcept {
    if(!std::isfinite(bpm)||bpm<1||bpm>1000)return false;
    anchor(position());bpm_=bpm;return true;
}
bool SampleClock::meter(int numerator,int denominator) noexcept {
    if(numerator<1||numerator>32||denominator<1||denominator>32||(denominator&(denominator-1)))return false;
    barLength_=numerator*4./denominator;return true;
}
bool SampleClock::seek(double beat) noexcept {
    if(!validBeat(beat))return false;
    anchor(beat);changed(ClockSeek);return true;
}
bool SampleClock::loop(bool enabled,double start,double end) noexcept {
    if(enabled&&(!validBeat(start)||!validBeat(end)||end-start<1./64))return false;
    looping_=enabled;if(enabled){loopStart_=start;loopEnd_=end;}return true;
}
void SampleClock::play(bool running) noexcept {
    if(running==playing_)return;
    anchor(position());playing_=running;changed(running?ClockStart:ClockStop);
}
ClockFrame SampleClock::next() noexcept {
    double beat=position();
    if(playing_&&looping_&&beat>=loopEnd_){
        beat=loopStart_+std::fmod(beat-loopStart_,loopEnd_-loopStart_);
        anchor(beat);changed(ClockLoop);
    }
    const ClockFrame frame{sample_,revision_,beat,bpm_,std::floor(beat/barLength_)*barLength_,barLength_,changes_,playing_};
    changes_=ClockNone;++sample_;return frame;
}
bool GridClock::division(double beats) noexcept {
    if(!std::isfinite(beats)||beats<1./64||beats>64)return false;
    if(beats!=division_){division_=beats;seen_=false;}return true;
}
GridPulse GridClock::consume(const ClockFrame& f) noexcept {
    GridPulse p;
    if(!validBeat(f.beat)){seen_=false;return p;}
    // Only correct tiny arithmetic error at an exact boundary, never move an
    // arbitrary near-boundary event by a whole sample.
    const double grid=f.beat/division_, nearest=std::round(grid);
    const bool exact=std::abs(grid-nearest)<=1e-10;
    const int64_t index=int64_t(exact?nearest:std::floor(grid));
    p.index=index;p.reset=!seen_||revision_!=f.revision;
    p.tick=f.playing&&(p.reset?exact:index!=index_);
    seen_=true;revision_=f.revision;index_=index;return p;
}

void NoteEventBlock::begin(uint32_t frames) noexcept {
    frames_=frames;size_=0;failed_=frames>maximumEventBlockFrames;
}
bool NoteEventBlock::push(const NoteEvent& event) noexcept {
    if(failed_)return false;
    if(event.offset>=frames_||!validEvent(event)||size_==events_.size()){fail();return false;}
    size_t i=size_++;
    while(i>0&&before(event,events_[i-1])){events_[i]=events_[i-1];--i;}
    events_[i]=event;return true;
}
void HeldNotes::clear(uint32_t source) noexcept {
    for(auto& n:notes_)if(n.active&&(source==allNoteSources||n.source==source)){n.active=false;--size_;}
}
NoteChange HeldNotes::apply(const NoteEvent& e) noexcept {
    if(muted_)return NoteChange::Ignored;
    if(!validEvent(e)){clear();muted_=true;return NoteChange::Overflow;}
    if(e.kind==NoteEventKind::Reset){clear(e.source);return NoteChange::Reset;}
    HeldNote* found=nullptr;HeldNote* free=nullptr;
    for(auto& n:notes_){
        if(n.active&&n.source==e.source&&n.noteId==e.noteId)found=&n;
        if(!n.active&&!free)free=&n;
    }
    if(e.kind==NoteEventKind::Off){if(!found)return NoteChange::Ignored;found->active=false;--size_;return NoteChange::Released;}
    if(!found&&!free){clear();muted_=true;return NoteChange::Overflow;}
    const bool updated=found!=nullptr;
    if(!found){found=free;++size_;}
    *found={e.source,e.noteId,++order_,e.pitch,e.velocity,true};
    return updated?NoteChange::Updated:NoteChange::Added;
}
const HeldNote* HeldNotes::newest() const noexcept {
    const HeldNote* latest=nullptr;
    for(const auto& n:notes_)if(n.active&&(!latest||n.order>latest->order))latest=&n;
    return latest;
}
} // namespace optisynth
