#pragma once

#include "intervals.hpp"
#include "models.hpp"
#include "recurrence.hpp"
#include "scorer.hpp"
#include <algorithm>
#include <queue>
#include <vector>

namespace scheduler {

inline void prepare_resource(Resource& resource, int lo, int hi, bool include_off_hours = true) {
    auto raw = expand_resource_busy(resource, lo, hi, include_off_hours);
    resource.normalized_busy = merge_intervals(std::move(raw));
}

inline bool has_all_equipment(
    const std::set<std::string>& room_equip, const std::set<std::string>& req_equip
) {
    return std::includes(
        room_equip.begin(), room_equip.end(),
        req_equip.begin(), req_equip.end()
    );
}

inline std::vector<Room*> filter_rooms(
    std::vector<Room>& rooms, int attendee_count, const std::set<std::string>& equipment
) {
    std::vector<Room*> eligible;
    for (auto& r : rooms) {
        if (r.capacity >= attendee_count && has_all_equipment(r.equipment, equipment)) {
            eligible.push_back(&r);
        }
    }
    return eligible;
}

inline std::vector<ScoredSlot> search_slots(
    const SlotRequest& req,
    std::vector<Room>& rooms,
    const ScoringWeights& weights = ScoringWeights{},
    bool include_off_hours = true
) {
    auto [lo, hi] = req.search_window;
    int d = req.duration;
    if (hi - lo < d || d <= 0) return {};

    // 1. Filter rooms by capacity and equipment
    auto eligible_rooms = filter_rooms(rooms, req.attendee_count, req.equipment);
    if (eligible_rooms.empty()) return {};

    // 2. Expand & normalize busy lists for all relevant resources
    for (auto* fac : req.required_faculty) {
        prepare_resource(*fac, lo, hi, include_off_hours);
    }
    for (auto* fac : req.optional_faculty) {
        prepare_resource(*fac, lo, hi, include_off_hours);
    }
    for (auto* rm : eligible_rooms) {
        prepare_resource(*rm, lo, hi, include_off_hours);
    }

    // 3. Step C: Union required faculty busy lists
    std::vector<std::vector<Interval>> req_lists;
    req_lists.reserve(req.required_faculty.size());
    for (const auto* fac : req.required_faculty) {
        req_lists.push_back(fac->normalized_busy);
    }
    auto people_busy = union_busy(req_lists);

    // 4. Size-k min-heap to track top-k candidates in O(C log k)
    std::priority_queue<ScoredSlot, std::vector<ScoredSlot>, std::greater<ScoredSlot>> min_heap;

    for (const auto* room : eligible_rooms) {
        auto combined_busy = union_busy({people_busy, room->normalized_busy});
        auto free_windows = gaps(combined_busy, lo, hi, d);

        for (const auto& win : free_windows) {
            auto starts = enumerate_starts(win, d, req.granularity);
            for (int t : starts) {
                auto [score, reasons] = score_candidate(t, *room, req, combined_busy, weights);
                ScoredSlot candidate{t, t + d, room->id, room->name, score, std::move(reasons)};

                if (min_heap.size() < req.top_k) {
                    min_heap.push(std::move(candidate));
                } else if (candidate > min_heap.top()) {
                    min_heap.pop();
                    min_heap.push(std::move(candidate));
                }
            }
        }
    }

    // Extract and sort descending by score (and ascending by start time on ties)
    std::vector<ScoredSlot> results;
    results.reserve(min_heap.size());
    while (!min_heap.empty()) {
        results.push_back(min_heap.top());
        min_heap.pop();
    }
    std::reverse(results.begin(), results.end());
    return results;
}

// Section 5: Guaranteed no-clash atomic booking check in O(log n) per resource
inline bool book_slot(Room& room, const std::vector<Faculty*>& attendees, int start, int end) {
    std::vector<Resource*> resources;
    resources.reserve(1 + attendees.size());
    resources.push_back(&room);
    for (auto* f : attendees) resources.push_back(f);

    // Phase 1: Re-validate every resource is clash-free
    for (const auto* res : resources) {
        if (!can_book(res->normalized_busy, start, end)) {
            return false;
        }
    }

    // Phase 2: Atomic insert into both one_off_busy and sorted normalized_busy
    for (auto* res : resources) {
        res->one_off_busy.emplace_back(start, end);
        auto it = std::lower_bound(
            res->normalized_busy.begin(), res->normalized_busy.end(), start,
            [](const Interval& iv, int val) { return iv.first < val; }
        );
        res->normalized_busy.insert(it, {start, end});
    }
    return true;
}

} // namespace scheduler