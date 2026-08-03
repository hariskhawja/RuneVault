// notification_log.h
// RuneVault — the per-turn NOTIFICATION LOG singleton (feature: turn log).
//
// A single process-wide message buffer any module can push human-readable
// strings to during a turn (the Model pushes on moves, attacks, pickups,
// scroll use, and descents). The Game owns the print+clear LIFECYCLE: after it
// processes a command, redraws the board, and prints the status, it appends a
// "Notifications:" section listing each buffered message on its own line and
// then CLEARS the log so the next turn starts empty.
//
// The log is a classic Meyers singleton — a single shared buffer is exactly the
// point (any component can `notify(...)` without threading a reference through
// the whole call graph). Because the buffer is global/process-wide, TESTS must
// call clear() at the START of each test to avoid cross-test contamination.

#ifndef RUNEVAULT_NOTIFICATION_LOG_H
#define RUNEVAULT_NOTIFICATION_LOG_H

#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

namespace runevault {

// Process-wide, append-only-until-cleared message buffer for one turn's worth
// of notifications. The Game prints and clears it every redraw (see header
// note above); everything else only ever push()es to it.
class NotificationLog {
public:
    // The single shared instance (Meyers singleton — constructed on first use,
    // destroyed at program exit).
    static NotificationLog &instance() {
        static NotificationLog log;
        return log;
    }

    // Append a message to the end of the current turn's buffer.
    void push(const std::string &message) { messages_.push_back(message); }

    // Read-only access to the buffered messages, in push order.
    const std::vector<std::string> &messages() const { return messages_; }

    // The buffered messages prepared for output. All messages are preserved
    // as-is (including duplicates) EXCEPT the "you have fallen" loss line, which
    // is collapsed to a single occurrence: because every Spectre that lands a
    // lethal-turn hit can push its own "You have fallen." line, that one message
    // is deduplicated so the loss is announced exactly once. Every other line —
    // moves, attacks, misses, pickups — is left untouched (repeats included).
    // This is what the Game prints each turn; `messages()` keeps the raw buffer.
    std::vector<std::string> displayMessages() const {
        std::vector<std::string> out;
        bool fallenSeen = false;
        for (const std::string &m : messages_) {
            if (containsFallen(m)) {
                if (fallenSeen) {
                    continue; // dedup ONLY the "you have fallen" line
                }
                fallenSeen = true;
            }
            out.push_back(m);
        }
        return out;
    }

    // Empty the buffer (the Game calls this after printing each turn's section).
    void clear() { messages_.clear(); }

    // True while no messages are buffered.
    bool empty() const { return messages_.empty(); }

private:
    NotificationLog() = default;
    NotificationLog(const NotificationLog &) = delete;
    NotificationLog &operator=(const NotificationLog &) = delete;

    // Case-insensitive test for whether `m` contains the loss phrase
    // "you have fallen" (the only line displayMessages() deduplicates).
    static bool containsFallen(const std::string &m) {
        static const std::string needle = "you have fallen";
        if (m.size() < needle.size()) {
            return false;
        }
        for (std::size_t i = 0; i + needle.size() <= m.size(); ++i) {
            bool match = true;
            for (std::size_t j = 0; j < needle.size(); ++j) {
                if (std::tolower(static_cast<unsigned char>(m[i + j])) !=
                    needle[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> messages_;
};

// Terse convenience for call sites: push a message onto the shared log without
// spelling out NotificationLog::instance().push(...) each time.
inline void notify(const std::string &m) {
    NotificationLog::instance().push(m);
}

} // namespace runevault

#endif // RUNEVAULT_NOTIFICATION_LOG_H
