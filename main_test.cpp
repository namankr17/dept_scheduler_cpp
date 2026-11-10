#include "engine.hpp"
#include <cassert>
#include <iostream>

using namespace scheduler;

void test_worked_example() {
    int lo = to_minutes(0, 9, 0);
    int hi = to_minutes(0, 18, 0);

    auto prof_a = merge_intervals({
        {to_minutes(0, 9, 0), to_minutes(0, 10, 0)},
        {to_minutes(0, 13, 0), to_minutes(0, 14, 0)}
    });
    auto prof_b = merge_intervals({
        {to_minutes(0, 10, 0), to_minutes(0, 11, 0)},
        {to_minutes(0, 13, 30), to_minutes(0, 15, 0)}
    });
    auto room_r = merge_intervals({
        {to_minutes(0, 11, 0), to_minutes(0, 12, 0)}
    });

    auto combined = union_busy({prof_a, prof_b, room_r});
    std::vector<Interval> expected_union = {
        {to_minutes(0, 9, 0), to_minutes(0, 12, 0)},
        {to_minutes(0, 13, 0), to_minutes(0, 15, 0)}
    };
    assert(combined == expected_union);

    auto free_wins = gaps(combined, lo, hi, 60);
    std::vector<Interval> expected_gaps = {
        {to_minutes(0, 12, 0), to_minutes(0, 13, 0)},
        {to_minutes(0, 15, 0), to_minutes(0, 18, 0)}
    };
    assert(free_wins == expected_gaps);

    std::vector<int> candidates;
    for (const auto& w : free_wins) {
        auto s = enumerate_starts(w, 60, 30);
        candidates.insert(candidates.end(), s.begin(), s.end());
    }

    std::vector<int> expected_starts = {
        to_minutes(0, 12, 0),
        to_minutes(0, 15, 0),
        to_minutes(0, 15, 30),
        to_minutes(0, 16, 0),
        to_minutes(0, 16, 30),
        to_minutes(0, 17, 0)
    };
    assert(candidates == expected_starts);
    std::cout << "[PASS] Worked Example from Spec\n";
}

void test_boundary_and_merge() {
    // Boundary test: [09:00, 10:00) and [10:00, 11:00) do not clash
    std::vector<Interval> busy = {{to_minutes(0, 9, 0), to_minutes(0, 10, 0)}};
    assert(can_book(busy, to_minutes(0, 10, 0), to_minutes(0, 11, 0)) == true);

    // Overlapping and duplicate busy intervals merge cleanly
    std::vector<Interval> raw = {{600, 660}, {540, 600}, {580, 640}, {600, 660}, {700, 720}};
    std::vector<Interval> expected = {{540, 660}, {700, 720}};
    assert(merge_intervals(raw) == expected);
    std::cout << "[PASS] Boundary & Merge Tests\n";
}

void test_pipeline_and_atomic_booking() {
    int lo = to_minutes(0, 9, 0);
    int hi = to_minutes(0, 12, 0);

    Faculty f1("F1", "Dr. Rao");
    f1.recurring_busy.push_back({0, 9 * 60, 10 * 60}); // Busy Mon 09:00-10:00

    std::vector<Room> rooms = {
        Room("R1", "Small Lab", 20, {"projector"}),
        Room("R2", "Old Hall", 50, {}),
        Room("R3", "LH-1", 45, {"projector", "mic"})
    };

    SlotRequest req{
        60,                  // 60 mins
        40,                  // 40 attendees
        {"projector"},       // needs projector
        {&f1},               // required faculty
        {},                  // optional faculty
        {lo, hi}             // Mon 09:00 - 12:00
    };

    auto results = search_slots(req, rooms);
    assert(!results.empty());
    for (const auto& slot : results) {
        assert(slot.room_id == "R3"); // Only R3 satisfies capacity >= 40 and projector
    }

    // Print top ranked slot and its explainable reasons
    const auto& best = results.front();
    std::cout << "[INFO] Top Slot: " << format_time(best.start) << " - " << format_time(best.end)
              << " in " << best.room_name << " (Score: " << best.score << ")\n";

    // Atomic booking test on R3 (rooms[2])
    assert(book_slot(rooms[2], {&f1}, best.start, best.end) == true);
    // Overlapping booking must be rejected
    assert(book_slot(rooms[2], {&f1}, best.start + 15, best.end + 15) == false);
    std::cout << "[PASS] Pipeline Filtering & Atomic Booking Tests\n";
}

int main() {
    test_worked_example();
    test_boundary_and_merge();
    test_pipeline_and_atomic_booking();
    std::cout << "\nAll C++ scheduler tests passed!\n";
    return 0;
}