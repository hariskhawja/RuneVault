#ifndef RUNEVAULT_SCROLL_TYPE_H
#define RUNEVAULT_SCROLL_TYPE_H

#include <string>

namespace runevault {

enum class ScrollType {
    AttuneFocus,  // +FP        (digit 0)
    SurgePower,   // +Pwr       (digit 1)
    FortifyWard,  // +WR        (digit 2)
    DrainFocus,   // -FP        (digit 3)
    SapPower,     // -Pwr       (digit 4)
    ErodeWard     // -WR        (digit 5)
};

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
