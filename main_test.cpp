#include "engine.hpp"
#include <cassert>
#include <iostream>

using namespace scheduler;

void test_worked_example() {
    int lo = to_minutes(0, 9, 0);
    int hi = to_minutes(0, 18, 0);

    // 1. Verify low-level interval primitives (Steps B, C, D, E)
    auto prof_a_iv = merge_intervals({
        {to_minutes(0, 9, 0), to_minutes(0, 10, 0)},
        {to_minutes(0, 13, 0), to_minutes(0, 14, 0)}
    });
    auto prof_b_iv = merge_intervals({
        {to_minutes(0, 10, 0), to_minutes(0, 11, 0)},
        {to_minutes(0, 13, 30), to_minutes(0, 15, 0)}
    });
    auto room_r_iv = merge_intervals({
        {to_minutes(0, 11, 0), to_minutes(0, 12, 0)}
    });

    auto combined = union_busy({prof_a_iv, prof_b_iv, room_r_iv});
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

    // 2. Run full scoring, ranking, and atomic booking on the Worked Example
    Faculty prof_a("FA", "Prof A");
    prof_a.one_off_busy = {
        {to_minutes(0, 9, 0), to_minutes(0, 10, 0)},
        {to_minutes(0, 13, 0), to_minutes(0, 14, 0)}
    };

    Faculty prof_b("FB", "Prof B");
    prof_b.one_off_busy = {
        {to_minutes(0, 10, 0), to_minutes(0, 11, 0)},
        {to_minutes(0, 13, 30), to_minutes(0, 15, 0)}
    };

    std::vector<Room> rooms = {
        Room("RR", "Room R", 30, {"projector"})
    };
    rooms[0].one_off_busy = {
        {to_minutes(0, 11, 0), to_minutes(0, 12, 0)}
    };

    SlotRequest req{
        60,                  // duration: 1 hour (60 mins)
        25,                  // attendee_count: 25
        {"projector"},       // equipment
        {&prof_a, &prof_b},  // required_faculty
        {},                  // optional_faculty
        {lo, hi},            // search_window: Mon 09:00 - 18:00
        1,                   // priority
        30,                  // granularity: 30 mins
        10                   // top_k
    };

    auto ranked = search_slots(req, rooms);
    assert(ranked.size() == 6);

    std::cout << "=== Worked Example Ranked Candidates ===\n";
    for (std::size_t i = 0; i < ranked.size(); ++i) {
        const auto& s = ranked[i];
        std::cout << "  #" << (i + 1) << ": " << format_time(s.start) << " - " << format_time(s.end)
                  << " | " << s.room_name << " | Score: " << s.score << " | Reasons: ";
        for (std::size_t r = 0; r < s.reasons.size(); ++r) {
            std::cout << s.reasons[r] << (r + 1 < s.reasons.size() ? ", " : "");
        }
        std::cout << "\n";
    }

    // Book the #1 best slot (15:00 - 16:00 wins over 12:00 because 12:00 overlaps lunch)
    const auto& best = ranked.front();
    assert(best.start == to_minutes(0, 15, 0));
    assert(best.end == to_minutes(0, 16, 0));

    bool booked = book_slot(rooms[0], {&prof_a, &prof_b}, best.start, best.end);
    assert(booked == true);
    std::cout << "[BOOKED] Best slot confirmed: " << format_time(best.start)
              << " - " << format_time(best.end) << " in " << best.room_name << "\n";

    // Re-running search_slots() now reflects the booked 15:00-16:00 slot
    auto remaining = search_slots(req, rooms);
    assert(remaining.size() == 4); // 15:00 and 15:30 are gone; 12:00, 16:00, 16:30, 17:00 remain
    std::cout << "[INFO] Remaining valid slots after booking: " << remaining.size()
              << " (Next best: " << format_time(remaining.front().start)
              << " - " << format_time(remaining.front().end)
              << ", Score: " << remaining.front().score << ")\n";

    std::cout << "[PASS] Worked Example from Spec\n\n";
    std::cout << "======================================\n\n";
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