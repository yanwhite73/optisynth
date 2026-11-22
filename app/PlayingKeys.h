#pragma once
#include <JuceHeader.h>
#include <ApplicationServices/ApplicationServices.h>
#include "../plugin/Source/TypingKeyboard.h"

namespace optiui {
inline bool capsLockOn() {
    return (CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState) & kCGEventFlagMaskAlphaShift) != 0;
}

inline bool canPlayTypingKeys(bool caps, juce::ModifierKeys mods, juce::Component* focus) {
    if (!caps || mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown()) return false;
    for (auto* c = focus; c; c = c->getParentComponent())
        if (dynamic_cast<juce::TextEditor*>(c) || dynamic_cast<juce::ComboBox*>(c)) return false;
    return true;
}

// Polling owns note-on/off. Claim the matching key-down here, including repeats,
// so AppKit does not fall through to its unhandled-key alert.
inline bool handlesPlayingKey(const juce::KeyPress& key, bool caps = capsLockOn(),
                             juce::Component* focus = juce::Component::getCurrentlyFocusedComponent()) {
    if (!canPlayTypingKeys(caps, key.getModifiers(), focus)) return false;
    const auto code = juce::CharacterFunctions::toUpperCase(key.getKeyCode());
    if (code == 'Z' || code == 'X') return true;
    for (char mapped : TypingKeyboard::keys)
        if (mapped && code == mapped) return true;
    return false;
}
}
