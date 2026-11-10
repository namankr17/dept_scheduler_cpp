#pragma once

#include "intervals.hpp"
#include "models.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace scheduler {

struct ScoringWeights {
    double time_of_day = 0.25;
    double room_fit    = 0.20;
    double tightness   = 0.15;
    double disruption  = 0.15;
    double earliness   = 0.15;
    double optional_f  = 0.10;
};

inline std::pair<double, std::string> time_of_day_score(int start, int end) {
    int s_tod = start % MINUTES_PER_DAY;
    int e_tod = s_tod + (end - start);

    int lunch_start = 12 * 60 + 30; // 12:30
    int lunch_end   = 14 * 60;      // 14:00
    if (s_tod < lunch_end && lunch_start < e_tod) {
        return {0.1, "overlaps lunch window"};
    }
    if ((s_tod >= 600 && e_tod <= 750) || (s_tod >= 840 && e_tod <= 990)) {
        return {1.0, "prime working hours"};
    }
    return {0.7, "avoids lunch"};
}

inline std::pair<double, std::string> room_fit_score(int n, int capacity) {
    if (capacity <= 0) return {0.0, ""};
    double ratio = std::min(1.0, static_cast<double>(n) / capacity);
    return {ratio, "fits " + std::to_string(n) + " of " + std::to_string(capacity) + " seats"};
}

inline std::pair<double, std::string> tightness_score(
    int start, int end, const std::vector<Interval>& combined_busy
) {
    if (combined_busy.empty()) return {0.5, "open schedule"};

    auto it = std::lower_bound(
        combined_busy.begin(), combined_busy.end(), start,
        [](const Interval& iv, int val) { return iv.first < val; }
    );

    int gap_before = (it != combined_busy.begin()) ? (start - std::prev(it)->second) : 9999;
    int gap_after  = (it != combined_busy.end())   ? (it->first - end) : 9999;
    int min_gap = std::min(gap_before, gap_after);

    if (min_gap == 0) {
        return {1.0, "back-to-back with adjacent commitment"};
    }
    return {std::max(0.0, 1.0 - (min_gap / 120.0)), ""};
}

inline std::pair<double, std::string> disruption_score(
    int start, int end, const std::vector<Faculty*>& required_faculty
) {
    if (required_faculty.empty()) return {1.0, ""};

    int broken_count = 0;
    for (const auto* fac : required_faculty) {
        const auto& busy = fac->normalized_busy;
        auto it = std::lower_bound(
            busy.begin(), busy.end(), start,
            [](const Interval& iv, int val) { return iv.first < val; }
        );
        int prev_end   = (it != busy.begin()) ? std::prev(it)->second : start;
        int next_start = (it != busy.end())   ? it->first : end;

        if ((start - prev_end) >= 30 && (next_start - end) >= 30) {
            ++broken_count;
        }
    }

    double clean_ratio = 1.0 - (static_cast<double>(broken_count) / required_faculty.size());
    std::string reason = (broken_count == 0) ? "preserves continuous free blocks" : "";
    return {clean_ratio, reason};
}

inline double earliness_score(int start, int lo, int hi, int d) {
    double span = std::max(1, (hi - d) - lo);
    return std::clamp(1.0 - (start - lo) / span, 0.0, 1.0);
}

inline std::pair<double, std::string> optional_faculty_score(
    int start, int end, const std::vector<Faculty*>& optional_faculty
) {
    if (optional_faculty.empty()) return {1.0, ""};
    int free_count = 0;
    for (const auto* fac : optional_faculty) {
        if (can_book(fac->normalized_busy, start, end)) {
            ++free_count;
        }
    }
    double frac = static_cast<double>(free_count) / optional_faculty.size();
    std::string reason = std::to_string(free_count) + "/" +
                         std::to_string(optional_faculty.size()) + " optional faculty free";
    return {frac, reason};
}

inline std::pair<double, std::vector<std::string>> score_candidate(
    int start,
    const Room& room,
    const SlotRequest& req,
    const std::vector<Interval>& combined_busy,
    const ScoringWeights& weights = ScoringWeights{}
) {
    int end = start + req.duration;
    auto [lo, hi] = req.search_window;

    auto [tod_val, tod_reason]     = time_of_day_score(start, end);
    auto [fit_val, fit_reason]     = room_fit_score(req.attendee_count, room.capacity);
    auto [tight_val, tight_reason] = tightness_score(start, end, combined_busy);
    auto [disr_val, disr_reason]   = disruption_score(start, end, req.required_faculty);
    double early_val               = earliness_score(start, lo, hi, req.duration);
    auto [opt_val, opt_reason]     = optional_faculty_score(start, end, req.optional_faculty);

    double total = weights.time_of_day * tod_val +
                   weights.room_fit    * fit_val +
                   weights.tightness   * tight_val +
                   weights.disruption  * disr_val +
                   weights.earliness   * early_val +
                   weights.optional_f  * opt_val;

    total = std::round(total * 10000.0) / 10000.0;

    std::vector<std::string> reasons;
    for (const auto& r : {
        fit_reason,
        tod_reason,
        tight_reason,
        disr_reason,
        "all " + std::to_string(req.required_faculty.size()) + " required faculty free",
        opt_reason
    }) {
        if (!r.empty()) reasons.push_back(r);
    }

    return {total, reasons};
}

} // namespace scheduler