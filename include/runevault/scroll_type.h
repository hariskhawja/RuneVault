// scroll_type.h
// RuneVault — the six Arcane Scroll types (shared enum).
//
// The six scroll types are shared across several components: the class Strategy
// hook `scrollEffectModifier(ScrollType, int)` (task 9.1, design "Arcanist:
// Factory Method + Strategy"), the scroll USE logic and Decorator chain (task
// 12.x), and the layout-file digit mapping (Requirement 12.2). To avoid a
// competing enum being introduced later, the type is DECLARED HERE now so the
// strategy signature can compile, and task 12.2 builds the scroll USE logic on
// top of this same header.
//
// The enumerator ORDER matches the layout-file digit mapping in Requirement
// 12.2 (0 = Attune Focus, 1 = Surge Power, 2 = Fortify Ward, 3 = Drain Focus,
// 4 = Sap Power, 5 = Erode Ward) and Requirement 8's effect list.

#ifndef RUNEVAULT_SCROLL_TYPE_H
#define RUNEVAULT_SCROLL_TYPE_H

#include <string>

namespace runevault {

// The six Arcane Scroll types (Requirement 8). "Positive" scrolls buff the
// Arcanist (AttuneFocus, SurgePower, FortifyWard); the trailing three are their
// harmful counterparts (DrainFocus, SapPower, ErodeWard) that the Voidwalker
// ability may flip to positive (Requirement 8.12).
enum class ScrollType {
    AttuneFocus,  // +FP        (digit 0)
    SurgePower,   // +Pwr       (digit 1)
    FortifyWard,  // +WR        (digit 2)
    DrainFocus,   // -FP        (digit 3)
    SapPower,     // -Pwr       (digit 4)
    ErodeWard     // -WR        (digit 5)
};

// Readable, human-facing name for a scroll type ("Attune Focus", "Surge
// Power", "Fortify Ward", "Drain Focus", "Sap Power", "Erode Ward"). Pure and
// standalone (header-only) — used by the notification log to phrase scroll-use
// messages naturally.
inline std::string scrollTypeName(ScrollType t) {
    switch (t) {
    case ScrollType::AttuneFocus:
        return "Attune Focus";
    case ScrollType::SurgePower:
        return "Surge Power";
    case ScrollType::FortifyWard:
        return "Fortify Ward";
    case ScrollType::DrainFocus:
        return "Drain Focus";
    case ScrollType::SapPower:
        return "Sap Power";
    case ScrollType::ErodeWard:
        return "Erode Ward";
    }
    // Unreachable for a valid ScrollType; keeps the compiler happy.
    return "Scroll";
}

} // namespace runevault

#endif // RUNEVAULT_SCROLL_TYPE_H
