#ifndef RUNEVAULT_NOTIFICATION_LOG_H
#define RUNEVAULT_NOTIFICATION_LOG_H

#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

namespace runevault {

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
