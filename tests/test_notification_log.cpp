// test_notification_log.cpp
// RuneVault — tests for the per-turn NOTIFICATION LOG singleton and the
// readable name helpers (feature: turn log).
//
// The NotificationLog is a process-wide singleton, so EVERY test clears it at
// the START to avoid cross-test contamination. Uses the tiny assert-based
// harness (test_util.h); no main() here.

#include "test_util.h"

#include "runevault/geometry.h"
#include "runevault/notification_log.h"
#include "runevault/scroll_type.h"
#include "runevault/spectre.h"

#include <string>

using namespace runevault;

namespace {

// Convenience: does `haystack` contain `needle`?
bool contains(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

// --- Singleton: push / messages / clear / identity / notify ------------------

RV_TEST(notification_log_push_then_messages_reflects_it) {
    NotificationLog &log = NotificationLog::instance();
    log.clear(); // singleton is global — start clean

    RV_CHECK(log.empty());
    log.push("hello");
    RV_CHECK(!log.empty());
    RV_CHECK(log.messages().size() == 1u);
    RV_CHECK(log.messages()[0] == "hello");

    log.push("world");
    RV_CHECK(log.messages().size() == 2u);
    RV_CHECK(log.messages()[1] == "world");
}

RV_TEST(notification_log_clear_empties) {
    NotificationLog &log = NotificationLog::instance();
    log.clear();

    log.push("a");
    log.push("b");
    RV_CHECK(!log.empty());
    log.clear();
    RV_CHECK(log.empty());
    RV_CHECK(log.messages().empty());
}

RV_TEST(notification_log_instance_returns_same_object) {
    NotificationLog::instance().clear();

    NotificationLog &a = NotificationLog::instance();
    NotificationLog &b = NotificationLog::instance();
    RV_CHECK(&a == &b);

    a.push("shared");
    // The second reference observes the same buffer.
    RV_CHECK(b.messages().size() == 1u);
    RV_CHECK(b.messages()[0] == "shared");
}

RV_TEST(notification_log_display_only_dedupes_fallen_line) {
    NotificationLog &log = NotificationLog::instance();
    log.clear();

    // Non-fallen duplicates are PRESERVED as-is; only "You have fallen." is
    // collapsed to a single occurrence (many Spectres can each announce it).
    log.push("The Wraith hits you for 5 damage.");
    log.push("The Wraith hits you for 5 damage."); // duplicate — KEPT
    log.push("You have fallen.");
    log.push("You have fallen.");                  // duplicate fallen — DROPPED
    log.push("You have fallen.");                  // duplicate fallen — DROPPED

    // Raw buffer keeps every push.
    RV_CHECK(log.messages().size() == 5u);

    const std::vector<std::string> shown = log.displayMessages();
    // Two identical Wraith lines are both kept; the three fallen lines collapse
    // to one. Order is preserved.
    RV_CHECK(shown.size() == 3u);
    RV_CHECK(shown[0] == "The Wraith hits you for 5 damage.");
    RV_CHECK(shown[1] == "The Wraith hits you for 5 damage.");
    RV_CHECK(shown[2] == "You have fallen.");
}

RV_TEST(notification_log_display_preserves_non_fallen_duplicates) {
    NotificationLog &log = NotificationLog::instance();
    log.clear();

    log.push("You move east.");
    log.push("You move east.");
    log.push("You move east.");

    // No fallen line present: everything is left exactly as-is.
    const std::vector<std::string> shown = log.displayMessages();
    RV_CHECK(shown.size() == 3u);
}

RV_TEST(notification_log_notify_helper_pushes) {
    NotificationLog::instance().clear();

    notify("via helper");
    RV_CHECK(NotificationLog::instance().messages().size() == 1u);
    RV_CHECK(NotificationLog::instance().messages()[0] == "via helper");
}

// --- Name helpers: directionName --------------------------------------------

RV_TEST(direction_name_returns_readable_strings) {
    RV_CHECK(directionName(Direction::No) == "north");
    RV_CHECK(directionName(Direction::So) == "south");
    RV_CHECK(directionName(Direction::Ea) == "east");
    RV_CHECK(directionName(Direction::We) == "west");
    RV_CHECK(directionName(Direction::Ne) == "north-east");
    RV_CHECK(directionName(Direction::Nw) == "north-west");
    RV_CHECK(directionName(Direction::Se) == "south-east");
    RV_CHECK(directionName(Direction::Sw) == "south-west");
}

// --- Name helpers: spectreTypeName ------------------------------------------

RV_TEST(spectre_type_name_returns_readable_strings) {
    RV_CHECK(spectreTypeName(SpectreType::Wraith) == "Wraith");
    RV_CHECK(spectreTypeName(SpectreType::Banshee) == "Banshee");
    RV_CHECK(spectreTypeName(SpectreType::Revenant) == "Revenant");
    RV_CHECK(spectreTypeName(SpectreType::Shade) == "Shade");
    RV_CHECK(spectreTypeName(SpectreType::Lich) == "Lich");
    RV_CHECK(spectreTypeName(SpectreType::VaultAnchor) == "Vault Anchor");
    RV_CHECK(spectreTypeName(SpectreType::SpecterLord) == "Specter Lord");
}

// --- Name helpers: scrollTypeName -------------------------------------------

RV_TEST(scroll_type_name_returns_readable_strings) {
    RV_CHECK(scrollTypeName(ScrollType::AttuneFocus) == "Attune Focus");
    RV_CHECK(scrollTypeName(ScrollType::SurgePower) == "Surge Power");
    RV_CHECK(scrollTypeName(ScrollType::FortifyWard) == "Fortify Ward");
    RV_CHECK(scrollTypeName(ScrollType::DrainFocus) == "Drain Focus");
    RV_CHECK(scrollTypeName(ScrollType::SapPower) == "Sap Power");
    RV_CHECK(scrollTypeName(ScrollType::ErodeWard) == "Erode Ward");
}

// --- Sanity: a composed message reads naturally ------------------------------

RV_TEST(notification_log_composed_message_reads_naturally) {
    NotificationLog::instance().clear();
    notify("You move " + directionName(Direction::Ea) + ".");
    RV_CHECK(contains(NotificationLog::instance().messages()[0], "You move east"));
}
