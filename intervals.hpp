#pragma once

#include "models.hpp"
#include <algorithm>
#include <queue>
#include <set>
#include <vector>

namespace scheduler {

// Step B: Sort by start and merge overlapping or touching half-open intervals. O(m log m)
inline std::vector<Interval> merge_intervals(std::vector<Interval> intervals) {
    if (intervals.empty()) return {};
    std::sort(intervals.begin(), intervals.end());

    std::vector<Interval> out;
    out.reserve(intervals.size());
    for (const auto& [s, e] : intervals) {
        if (e <= s) continue; // Ignore empty or invalid intervals
        if (!out.empty() && s <= out.back().second) {
            out.back().second = std::max(out.back().second, e);
        } else {
            out.emplace_back(s, e);
        }
    }
    return out;
}

// Step C: K-way merge of already-sorted busy lists in O(N log k)
inline std::vector<Interval> union_busy(const std::vector<std::vector<Interval>>& lists) {
    struct Node {
        Interval iv;
        std::size_t list_idx;
        std::size_t elem_idx;
        bool operator>(const Node& other) const {
            return iv > other.iv;
        }
    };

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
    for (std::size_t i = 0; i < lists.size(); ++i) {
        if (!lists[i].empty()) {
            pq.push({lists[i][0], i, 0});
        }
    }

    std::vector<Interval> out;
    while (!pq.empty()) {
        Node curr = pq.top();
        pq.pop();

        const auto& [s, e] = curr.iv;
        if (e > s) {
            if (!out.empty() && s <= out.back().second) {
                out.back().second = std::max(out.back().second, e);
            } else {
                out.emplace_back(s, e);
            }
        }

        if (curr.elem_idx + 1 < lists[curr.list_idx].size()) {
            pq.push({lists[curr.list_idx][curr.elem_idx + 1], curr.list_idx, curr.elem_idx + 1});
        }
    }
    return out;
}

// Step D: Find free windows [a, b) inside [lo, hi) of length >= d. O(N)
inline std::vector<Interval> gaps(const std::vector<Interval>& busy, int lo, int hi, int d) {
    std::vector<Interval> free_wins;
    int cur = lo;

    for (const auto& [s, e] : busy) {
        if (e <= cur) continue;
        if (s > hi) break;
        int clamped_s = std::min(s, hi);
        if (clamped_s > cur && (clamped_s - cur) >= d) {
            free_wins.emplace_back(cur, clamped_s);
        }
        cur = std::max(cur, e);
        if (cur >= hi) break;
    }
    if (hi - cur >= d) {
        free_wins.emplace_back(cur, hi);
    }
    return free_wins;
}

// Step E: Enumerate candidate start times inside [a, b) on grid `g` + clock alignment
inline std::vector<int> enumerate_starts(Interval window, int d, int g) {
    int a = window.first;
    int b = window.second;
    int latest_start = b - d;
    if (latest_start < a) return {};

    std::set<int> starts;
    for (int t = a; t <= latest_start; t += g) {
        starts.insert(t);
    }

    int first_aligned = ((a + g - 1) / g) * g;
    for (int t = first_aligned; t <= latest_start; t += g) {
        starts.insert(t);
    }

    return std::vector<int>(starts.begin(), starts.end());
}

// Section 5: O(log n) guaranteed no-clash check on a sorted, disjoint busy list
inline bool can_book(const std::vector<Interval>& busy, int s, int e) {
    auto it = std::lower_bound(
        busy.begin(), busy.end(), s,
        [](const Interval& iv, int val) { return iv.first < val; }
    );

    if (it != busy.begin() && std::prev(it)->second > s) {
        return false; // Previous interval overlaps
    }
    if (it != busy.end() && it->first < e) {
        return false; // Next interval overlaps
    }
    return true;
}

} // namespace scheduler