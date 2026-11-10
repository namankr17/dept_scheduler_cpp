#pragma once

#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace scheduler {

// Half-open interval [start, end) in integer minutes since Week 0, Monday 00:00
using Interval = std::pair<int, int>;

constexpr int MINUTES_PER_HOUR = 60;
constexpr int MINUTES_PER_DAY = 24 * 60;
constexpr int MINUTES_PER_WEEK = 7 * MINUTES_PER_DAY;

inline int to_minutes(int day, int hour, int minute = 0, int week = 0) {
    return week * MINUTES_PER_WEEK +
           day * MINUTES_PER_DAY +
           hour * MINUTES_PER_HOUR +
           minute;
}

inline std::string format_time(int m) {
    static const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    int week = m / MINUTES_PER_WEEK;
    int rem = m % MINUTES_PER_WEEK;
    int day = rem / MINUTES_PER_DAY;
    rem %= MINUTES_PER_DAY;
    int hour = rem / MINUTES_PER_HOUR;
    int minute = rem % MINUTES_PER_HOUR;

    std::ostringstream oss;
    oss << "W" << week << " " << days[day] << " "
        << std::setw(2) << std::setfill('0') << hour << ":"
        << std::setw(2) << std::setfill('0') << minute;
    return oss.str();
}

struct RecurringRule {
    int weekday;    // 0 = Monday .. 6 = Sunday
    int start_min;  // Minutes from 00:00 of that day (e.g., 10*60 = 600)
    int end_min;    // Minutes from 00:00 of that day (e.g., 11*60 = 660)
};

struct Resource {
    std::string id;
    std::string name;
    std::vector<Interval> one_off_busy;
    std::vector<RecurringRule> recurring_busy;
    int work_start_min = 9 * 60;   // Default 09:00
    int work_end_min = 18 * 60;    // Default 18:00

    // Cached normalized busy list for O(log n) binary search
    std::vector<Interval> normalized_busy;
};

struct Faculty : public Resource {
    Faculty(std::string id_, std::string name_) {
        id = std::move(id_);
        name = std::move(name_);
    }
};

struct Room : public Resource {
    int capacity = 0;
    std::set<std::string> equipment;

    Room(std::string id_, std::string name_, int cap, std::set<std::string> equip = {})
        : capacity(cap), equipment(std::move(equip)) {
        id = std::move(id_);
        name = std::move(name_);
    }
};

struct SlotRequest {
    int duration;                           // d (in minutes)
    int attendee_count;                     // n
    std::set<std::string> equipment;        // E
    std::vector<Faculty*> required_faculty; // F
    std::vector<Faculty*> optional_faculty; // O
    Interval search_window;                 // [lo, hi)
    int priority = 1;                       // p
    int granularity = 15;                   // g
    std::size_t top_k = 10;                 // k
};

struct ScoredSlot {
    int start;
    int end;
    std::string room_id;
    std::string room_name;
    double score;
    std::vector<std::string> reasons;

    // Comparator for min-heap: smallest score sits at the top so it gets evicted first.
    // On tie, later start time sits at top so earlier start time is kept.
    bool operator>(const ScoredSlot& other) const {
        if (std::abs(score - other.score) > 1e-9) {
            return score > other.score;
        }
        return start < other.start;
    }
};

} // namespace scheduler