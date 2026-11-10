#pragma once

#include "models.hpp"
#include <vector>

namespace scheduler {

// Step A: Expand recurring timetable and off-working hours strictly within [lo, hi)
inline std::vector<Interval> expand_resource_busy(
    const Resource& resource, int lo, int hi, bool include_off_hours = true
) {
    std::vector<Interval> raw;

    // 1. One-off events overlapping [lo, hi)
    for (const auto& [s, e] : resource.one_off_busy) {
        if (s < hi && lo < e) {
            raw.emplace_back(s, e);
        }
    }

    int start_day = lo / MINUTES_PER_DAY;
    int end_day = (hi > lo) ? (hi - 1) / MINUTES_PER_DAY : start_day;

    for (int day_idx = start_day; day_idx <= end_day; ++day_idx) {
        int day_origin = day_idx * MINUTES_PER_DAY;
        int weekday = day_idx % 7;

        // 2. Weekly timetable entries matching this weekday
        for (const auto& rule : resource.recurring_busy) {
            if (rule.weekday == weekday) {
                int s = day_origin + rule.start_min;
                int e = day_origin + rule.end_min;
                if (s < hi && lo < e) {
                    raw.emplace_back(s, e);
                }
            }
        }

        // 3. Time outside working hours treated as busy
        if (include_off_hours) {
            if (resource.work_start_min > 0) {
                int s = day_origin;
                int e = day_origin + resource.work_start_min;
                if (s < hi && lo < e) raw.emplace_back(s, e);
            }
            if (resource.work_end_min < MINUTES_PER_DAY) {
                int s = day_origin + resource.work_end_min;
                int e = day_origin + MINUTES_PER_DAY;
                if (s < hi && lo < e) raw.emplace_back(s, e);
            }
        }
    }

    return raw;
}

} // namespace scheduler