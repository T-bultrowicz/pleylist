#include "playlist.h"

#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <cassert>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

// ======================== Narzędzia testowe ========================

struct test_exception : std::exception {
    const char* what() const noexcept override {
        return "test_exception";
    }
};

// Typ T – utwór
struct TestTrack {
    int id{};
    std::string name;

    static bool throw_on_copy;
    static bool throw_on_compare;

    static int live_count;
    static int copy_count;
    static int move_count;
    static int compare_count;

    TestTrack() : id(0), name("default") {
        ++live_count;
    }

    TestTrack(int id_, std::string name_)
        : id(id_), name(std::move(name_)) {
        ++live_count;
    }

    TestTrack(TestTrack const& other)
        : id(other.id), name(other.name) {
        if (throw_on_copy) {
            throw test_exception{};
        }
        ++live_count;
        ++copy_count;
    }

    TestTrack(TestTrack&& other) noexcept
        : id(other.id), name(std::move(other.name)) {
        ++live_count;
        ++move_count;
    }

    TestTrack& operator=(TestTrack const& other) {
        if (this != &other) {
            if (throw_on_copy) {
                throw test_exception{};
            }
            id = other.id;
            name = other.name;
            ++copy_count;
        }
        return *this;
    }

    TestTrack& operator=(TestTrack&& other) noexcept {
        if (this != &other) {
            id = other.id;
            name = std::move(other.name);
            ++move_count;
        }
        return *this;
    }

    ~TestTrack() {
        --live_count;
    }

    friend bool operator==(TestTrack const& a, TestTrack const& b) {
        if (throw_on_compare) {
            ++compare_count;
            throw test_exception{};
        }
        ++compare_count;
        return a.id == b.id;
    }

    friend bool operator<(TestTrack const& a, TestTrack const& b) {
        if (throw_on_compare) {
            ++compare_count;
            throw test_exception{};
        }
        ++compare_count;
        return a.id < b.id;
    }

    friend bool operator!=(TestTrack const& a, TestTrack const& b) {
        return !(a == b);
    }
    friend bool operator>(TestTrack const& a, TestTrack const& b) {
        return b < a;
    }
    friend bool operator<=(TestTrack const& a, TestTrack const& b) {
        return !(b < a);
    }
    friend bool operator>=(TestTrack const& a, TestTrack const& b) {
        return !(a < b);
    }

    static void reset_counters() {
        live_count   = 0;
        copy_count   = 0;
        move_count   = 0;
        compare_count = 0;
        throw_on_copy = false;
        throw_on_compare = false;
    }
};

bool TestTrack::throw_on_copy    = false;
bool TestTrack::throw_on_compare = false;
int TestTrack::live_count        = 0;
int TestTrack::copy_count        = 0;
int TestTrack::move_count        = 0;
int TestTrack::compare_count     = 0;

// Typ P – parametry
struct TestParams {
    int volume{};
    int tag{};

    static bool throw_on_copy;

    static int live_count;
    static int copy_count;

    TestParams() : volume(0), tag(0) {
        ++live_count;
    }

    TestParams(int v, int t) : volume(v), tag(t) {
        ++live_count;
    }

    TestParams(TestParams const& other)
        : volume(other.volume), tag(other.tag) {
        if (throw_on_copy) {
            throw test_exception{};
        }
        ++live_count;
        ++copy_count;
    }

    TestParams& operator=(TestParams const& other) {
        if (this != &other) {
            if (throw_on_copy) {
                throw test_exception{};
            }
            volume = other.volume;
            tag    = other.tag;
            ++copy_count;
        }
        return *this;
    }
    friend bool operator==(TestParams const& a, TestParams const&b) {
        if (a.tag != b.tag || a.volume != b.volume) return false;
        return true;

    }

    ~TestParams() {
        --live_count;
    }

    static void reset_counters() {
        live_count = 0;
        copy_count = 0;
        throw_on_copy = false;
    }
};

bool TestParams::throw_on_copy = false;
int  TestParams::live_count    = 0;
int  TestParams::copy_count    = 0;

using playlist_t = cxx::playlist<TestTrack, TestParams>;
using play_it    = typename playlist_t::play_iterator;
using sorted_it  = typename playlist_t::sorted_iterator;

static void reset_all_counters() {
    TestTrack::reset_counters();
    TestParams::reset_counters();
}

// pomoc do front()
static std::pair<TestTrack const*, TestParams const*>
get_front_ptrs(playlist_t& pl) {
    auto pr = pl.front();
    return { &pr.first, &pr.second };
}

// ======================== TESTY ========================

// 01: pusta plejlista – front i pop_front rzucają std::out_of_range
void test_01_empty_exceptions() {
    std::clog << "[01] empty front/pop_front\n";
    reset_all_counters();
    playlist_t pl;
    assert(pl.size() == 0);

    bool thrown = false;
    try {
        (void)pl.front();
    } catch (std::out_of_range const&) {
        thrown = true;
    }
    assert(thrown);

    thrown = false;
    try {
        pl.pop_front();
    } catch (std::out_of_range const&) {
        thrown = true;
    }
    assert(thrown);
}

// 02: push_back + front + size + kolejność
void test_02_basic_push_front() {
    std::clog << "[02] basic push + front\n";
    reset_all_counters();
    playlist_t pl;

    TestTrack t1{1, "one"};
    TestTrack t2{2, "two"};
    TestParams p1{10, 100};
    TestParams p2{20, 200};

    pl.push_back(t1, p1);
    pl.push_back(t2, p2);
    assert(pl.size() == 2);

    auto fr = pl.front();
    assert(fr.first.id == 1);
    assert(fr.second.volume == 10);
}

// 03: pop_front usuwa pierwszy element, kolejność zachowana
void test_03_pop_front_order() {
    std::clog << "[03] pop_front order\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "a"}, {1, 1});
    pl.push_back({2, "b"}, {2, 2});
    pl.push_back({3, "c"}, {3, 3});

    assert(pl.size() == 3);
    pl.pop_front();
    assert(pl.size() == 2);

    auto fr = pl.front();
    assert(fr.first.id == 2);
    assert(fr.second.volume == 2);
}

// 04: clear czyści wszystko, iteratory puste
void test_04_clear() {
    std::clog << "[04] clear\n";
    reset_all_counters();
    playlist_t pl;
    for (int i = 0; i < 5; ++i) {
        pl.push_back({i, "t"}, {i, i});
    }
    assert(pl.size() == 5);
    pl.clear();
    assert(pl.size() == 0);
    assert(pl.play_begin()   == pl.play_end());
    assert(pl.sorted_begin() == pl.sorted_end());
}

// 05: remove usuwa wszystkie wystąpienia T
void test_05_remove_all_occurrences() {
    std::clog << "[05] remove all occurrences\n";
    reset_all_counters();
    playlist_t pl;
    TestTrack a{1, "A"};
    TestTrack b{2, "B"};

    pl.push_back(a, {1, 10});
    pl.push_back(b, {2, 20});
    pl.push_back(a, {3, 30});
    pl.push_back(a, {4, 40});
    pl.push_back(b, {5, 50});

    assert(pl.size() == 5);
    pl.remove(a);
    assert(pl.size() == 2);

    for (auto it = pl.play_begin(); it != pl.play_end(); ++it) {
        auto pr = pl.play(it);
        assert(pr.first.id == 2);
    }
}

// 06: remove rzuca std::invalid_argument gdy brak T
void test_06_remove_throws_if_missing() {
    std::clog << "[06] remove throws\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {1, 1});
    pl.push_back({2, "B"}, {2, 2});

    bool thrown = false;
    try {
        pl.remove(TestTrack{3, "C"});
    } catch (std::invalid_argument const&) {
        thrown = true;
    }
    assert(thrown);
}

// 07: play_iterator – kolejność, pre/post++
void test_07_play_iterator_sequence() {
    std::clog << "[07] play_iterator sequence\n";
    reset_all_counters();
    playlist_t pl;
    for (int i = 0; i < 4; ++i) {
        pl.push_back({i, "T"}, {i, 10 + i});
    }

    // pre++
    {
        auto it  = pl.play_begin();
        auto end = pl.play_end();
        int expected = 0;
        while (it != end) {
            auto pr = pl.play(it);
            assert(pr.first.id == expected);
            ++it;
            ++expected;
        }
        assert(expected == 4);
    }

    // post++
    {
        auto it  = pl.play_begin();
        auto end = pl.play_end();
        int expected = 0;
        while (it != end) {
            auto it_old = it++;
            auto pr     = pl.play(it_old);
            assert(pr.first.id == expected);
            ++expected;
        }
        assert(expected == 4);
    }
}

// 08: sorted_iterator – unikalne i posortowane
void test_08_sorted_iterator_unique_sorted() {
    std::clog << "[08] sorted_iterator unique + sorted\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({10, "A"}, {0, 0});
    pl.push_back({5,  "B"}, {0, 0});
    pl.push_back({10, "A"}, {0, 0});
    pl.push_back({7,  "C"}, {0, 0});
    pl.push_back({5,  "B"}, {0, 0});

    std::vector<int> ids;
    for (auto it = pl.sorted_begin(); it != pl.sorted_end(); ++it) {
        auto pr = pl.pay(it);
        ids.push_back(pr.first.id);
    }

    assert(ids.size() == 3);
    assert(ids[0] == 5);
    assert(ids[1] == 7);
    assert(ids[2] == 10);
}

// 09: pay – poprawne liczniki
void test_09_pay_counts() {
    std::clog << "[09] pay counts\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {0, 0});
    pl.push_back({1, "A"}, {0, 1});
    pl.push_back({2, "B"}, {0, 2});
    pl.push_back({1, "A"}, {0, 3});
    pl.push_back({2, "B"}, {0, 4});

    for (auto it = pl.sorted_begin(); it != pl.sorted_end(); ++it) {
        auto pr = pl.pay(it);
        if (pr.first.id == 1) {
            assert(pr.second == 3);
        } else if (pr.first.id == 2) {
            assert(pr.second == 2);
        } else {
            assert(false);
        }
    }
}

// 10: COW – przed modyfikacją front współdzielony
void test_10_cow_before_write() {
    std::clog << "[10] COW before write\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 1});
    p1.push_back({2, "B"}, {20, 2});

    playlist_t p2 = p1;

    auto f1 = p1.front();
    auto f2 = p2.front();
    assert(&f1.first == &f2.first);
    assert(&f1.second == &f2.second);
}

// 11: COW – push_back odłącza dane
void test_11_cow_after_push_back() {
    std::clog << "[11] COW after push_back\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 1});

    playlist_t p2 = p1;

    auto f1b = p1.front();
    auto f2b = p2.front();
    assert(&f1b.first == &f2b.first);
    assert(&f1b.second == &f2b.second);

    p1.push_back({2, "B"}, {20, 2});

    auto f1a = p1.front();
    auto f2a = p2.front();
    // powinny już być różne adresy (brak współdzielenia)
    assert(&f1a.first != &f2a.first || &f1a.second != &f2a.second);
}

// 12: operator= z argumentem przez wartość + self-assignment
void test_12_assignment_and_self_assignment() {
    std::clog << "[12] assignment + self-assignment\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 1});
    p1.push_back({2, "B"}, {20, 2});

    playlist_t p2;
    p2.push_back({3, "C"}, {30, 3});

    p2 = p1;
    assert(p2.size() == p1.size());

    p1 = p1; // self-assignment
    assert(p1.size() == 2);
}

// 13: non-const params odłącza COW
void test_13_params_nonconst_detach() {
    std::clog << "[13] non-const params detaches\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 1});

    playlist_t p2 = p1;

    auto it1 = p1.play_begin();
    auto it2 = p2.play_begin();

    {
        auto pr1 = p1.play(it1);
        auto pr2 = p2.play(it2);
        assert(&pr1.second == &pr2.second);
    }

    // modyfikacja przez non-const params
    TestParams& ref = p1.params(it1);
    ref.volume = 99;

    auto pr1a = p1.play(p1.play_begin());
    auto pr2a = p2.play(it2);

    assert(pr1a.second.volume == 99);
    assert(pr2a.second.volume != 99);
    assert(&pr1a.second != &pr2a.second);
}

// 14: const params nie odłącza COW
void test_14_params_const_keeps_sharing() {
    std::clog << "[14] const params keeps sharing\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 1});

    playlist_t p2 = p1;

    auto it1 = p1.play_begin();
    auto it2 = p2.play_begin();

    playlist_t const& c1 = p1;
    playlist_t const& c2 = p2;

    auto& rp1 = c1.params(it1);
    auto& rp2 = c2.params(it2);

    assert(&rp1 == &rp2);

    auto pr1 = p1.play(it1);
    auto pr2 = p2.play(it2);
    assert(&pr1.second == &pr2.second);
}

// 15: push_back – wyjątek w kopiowaniu params, stan bez zmian
void test_15_push_back_exception_params_copy() {
    std::clog << "[15] push_back strong guarantee (params)\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});
    pl.push_back({2, "B"}, {20, 2});

    auto size_before = pl.size();
    auto [t_before, p_before] = get_front_ptrs(pl);

    TestParams bad{30, 3};
    TestParams::throw_on_copy = true;

    bool thrown = false;
    try {
        pl.push_back({3, "C"}, bad);
    } catch (test_exception const&) {
        thrown = true;
    }
    TestParams::throw_on_copy = false;
    assert(thrown);
    assert(pl.size() == size_before);

    auto [t_after, p_after] = get_front_ptrs(pl);
    assert(t_before == t_after);
    assert(p_before == p_after);
}

// 16: remove – wyjątek w porównaniu, stan bez zmian
void test_16_remove_exception_compare() {
    std::clog << "[16] remove strong guarantee (compare)\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});
    pl.push_back({2, "B"}, {20, 2});
    pl.push_back({3, "C"}, {30, 3});

    auto size_before = pl.size();
    TestTrack::throw_on_compare = true;
    bool thrown = false;
    try {
        pl.remove(TestTrack{2, "B"});
    } catch (test_exception const&) {
        thrown = true;
    }
    TestTrack::throw_on_compare = false;
    assert(thrown);
    assert(pl.size() == size_before);
}

// 17: iteratory play – ==, !=, dojście do end
void test_17_play_iterator_compare() {
    std::clog << "[17] play_iterator compare\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});
    pl.push_back({2, "B"}, {20, 2});

    auto it  = pl.play_begin();
    auto it2 = pl.play_begin();
    auto end = pl.play_end();

    assert(it == it2);
    ++it2;
    assert(it != it2);

    ++it;
    ++it;
    assert(it == end);
    assert(!(it != end));
}

// 18: iteratory sorted – ==, !=, dojście do end
void test_18_sorted_iterator_compare() {
    std::clog << "[18] sorted_iterator compare\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {0, 0});
    pl.push_back({2, "B"}, {0, 0});
    pl.push_back({1, "A"}, {0, 0});

    auto it  = pl.sorted_begin();
    auto it2 = pl.sorted_begin();
    auto end = pl.sorted_end();

    assert(it == it2);
    ++it2;
    assert(it != it2);

    ++it;
    ++it;
    assert(it == end);
}

// 19: nieudany push_back nie unieważnia istniejących play_iteratorów
void test_19_failed_push_back_keeps_play_iterator() {
    std::clog << "[19] failed push_back keeps play_iterator\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});
    pl.push_back({2, "B"}, {20, 2});

    auto it = pl.play_begin();
    auto pr_before = pl.play(it);
    assert(pr_before.first.id == 1);

    TestParams::throw_on_copy = true;
    bool thrown = false;
    try {
        pl.push_back({3, "C"}, {30, 3});
    } catch (test_exception const&) {
        thrown = true;
    }
    TestParams::throw_on_copy = false;
    assert(thrown);

    auto pr_after = pl.play(it);
    assert(pr_after.first.id == 1);
    assert(pr_after.second.volume == 10);
}

// 20: nieudany remove nie unieważnia istniejących sorted_iteratorów
void test_20_failed_remove_keeps_sorted_iterator() {
    std::clog << "[20] failed remove keeps sorted_iterator\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});
    pl.push_back({2, "B"}, {20, 2});

    auto it = pl.sorted_begin();
    auto pr_before = pl.pay(it);
    assert(pr_before.first.id == 1);

    TestTrack::throw_on_compare = true;
    bool thrown = false;
    try {
        pl.remove(TestTrack{2, "B"});
    } catch (test_exception const&) {
        thrown = true;
    }
    TestTrack::throw_on_compare = false;
    assert(thrown);

    auto pr_after = pl.pay(it);
    assert(pr_after.first.id == 1);
}

// 21: masywne kopiowanie i COW (N=10k, 20 kopii)
void test_21_large_n_and_many_copies() {
    std::clog << "[21] large N + many copies\n";
    reset_all_counters();
    playlist_t base;
    const int N = 10000;
    for (int i = 0; i < N; ++i) {
        base.push_back({i, "T"}, {i, i});
    }

    std::vector<playlist_t> copies;
    copies.reserve(20);
    for (int i = 0; i < 20; ++i) {
        copies.push_back(base);
    }

    for (auto& pl : copies) {
        assert(pl.size() == base.size());
        auto it = pl.play_begin();
        auto pr = pl.play(it);
        assert(pr.first.id == 0);
    }
}

// 22: COW + wyjątek w push_back – brak odłączenia przy nieudanej modyfikacji
void test_22_cow_and_failed_push_back() {
    std::clog << "[22] COW + failed push_back no detach\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 1});

    playlist_t p2 = p1;

    auto f1b = p1.front();
    auto f2b = p2.front();
    assert(&f1b.first == &f2b.first);
    assert(&f1b.second == &f2b.second);

    TestParams::throw_on_copy = true;
    bool thrown = false;
    try {
        p1.push_back({2, "B"}, {20, 2});
    } catch (test_exception const&) {
        thrown = true;
    }
    TestParams::throw_on_copy = false;
    assert(thrown);

    auto f1a = p1.front();
    auto f2a = p2.front();
    // wciąż współdzielone
    assert(&f1a.first == &f2a.first);
    assert(&f1a.second == &f2a.second);
}

// 23: const playlist – iteracja, pay, play
void test_23_const_playlist_usage() {
    std::clog << "[23] const playlist usage\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});
    pl.push_back({2, "B"}, {20, 2});

    playlist_t const& cpl = pl;

    int count = 0;
    for (auto it = cpl.play_begin(); it != cpl.play_end(); ++it) {
        auto pr = cpl.play(it);
        (void)pr;
        ++count;
    }
    assert(count == 2);

    int uniq = 0;
    for (auto it = cpl.sorted_begin(); it != cpl.sorted_end(); ++it) {
        auto pr = cpl.pay(it);
        (void)pr;
        ++uniq;
    }
    assert(uniq == 2);
}

// 24: przypisanie z tymczasowego (copy-and-swap)
playlist_t make_temp_playlist() {
    playlist_t tmp;
    tmp.push_back({42, "X"}, {1, 2});
    tmp.push_back({43, "Y"}, {3, 4});
    return tmp;
}

void test_24_assignment_from_temporary() {
    std::clog << "[24] assignment from temporary\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});

    pl = make_temp_playlist();
    assert(pl.size() == 2);

    auto it = pl.play_begin();
    auto pr = pl.play(it);
    assert(pr.first.id == 42);
}

// 25: sorted_iterator nie zależy od kolejności wstawiania
void test_25_sorted_order_independent_of_insertion() {
    std::clog << "[25] sorted independent of insertion\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({5, "E"}, {0, 0});
    pl.push_back({1, "A"}, {0, 0});
    pl.push_back({3, "C"}, {0, 0});
    pl.push_back({2, "B"}, {0, 0});
    pl.push_back({4, "D"}, {0, 0});

    std::vector<int> ids;
    for (auto it = pl.sorted_begin(); it != pl.sorted_end(); ++it) {
        auto pr = pl.pay(it);
        ids.push_back(pr.first.id);
    }

    assert(ids.size() == 5);
    for (int i = 0; i < 5; ++i) {
        assert(ids[i] == i + 1);
    }
}

// 26: wielokrotne łańcuchowe COW (p1, p2=p1, p3=p2, modyfikacja p2)
void test_26_multiple_cow_chain() {
    std::clog << "[26] multiple COW chain\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 0});

    playlist_t p2 = p1;
    playlist_t p3 = p2;

    auto it1 = p1.play_begin();
    auto it2 = p2.play_begin();
    auto it3 = p3.play_begin();

    auto pr1 = p1.play(it1);
    auto pr2 = p2.play(it2);
    auto pr3 = p3.play(it3);

    assert(&pr1.second == &pr2.second);
    assert(&pr2.second == &pr3.second);

    TestParams& ref2 = p2.params(it2);
    ref2.tag = 77;

    auto pr1a = p1.play(it1);
    auto pr2a = p2.play(p2.play_begin());
    auto pr3a = p3.play(it3);

    assert(&pr2a.second != &pr1a.second);
    assert(&pr2a.second != &pr3a.second);
    assert(&pr1a.second == &pr3a.second);
}

// 27: pop_front aż do pustej listy, potem wyjątek
void test_27_pop_front_until_empty() {
    std::clog << "[27] pop_front until empty\n";
    reset_all_counters();
    playlist_t pl;
    for (int i = 0; i < 5; ++i) {
        pl.push_back({i, "T"}, {i, i});
    }
    assert(pl.size() == 5);

    while (pl.size() > 0) {
        pl.pop_front();
    }
    assert(pl.size() == 0);

    bool thrown = false;
    try {
        pl.pop_front();
    } catch (std::out_of_range const&) {
        thrown = true;
    }
    assert(thrown);
}

// 28: mix push_back+pop_front+remove – sanity check
void test_28_mixed_operations() {
    std::clog << "[28] mixed operations\n";
    reset_all_counters();
    playlist_t pl;
    TestTrack a{1, "A"};
    TestTrack b{2, "B"};
    TestTrack c{3, "C"};

    pl.push_back(a, {1, 0});
    pl.push_back(b, {2, 0});
    pl.push_back(c, {3, 0});
    pl.push_back(a, {4, 0});

    assert(pl.size() == 4);
    pl.pop_front();          // usuwa A
    assert(pl.size() == 3);

    pl.remove(b);            // usuwa B
    assert(pl.size() == 2);

    auto it = pl.play_begin();
    auto pr1 = pl.play(it);
    assert(pr1.first.id == 3);
    ++it;
    auto pr2 = pl.play(it);
    assert(pr2.first.id == 1);
}

// 29: pay nie zmienia stanu (wielokrotne wywołanie)
void test_29_pay_is_read_only() {
    std::clog << "[29] pay is read-only\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {0, 0});
    pl.push_back({1, "A"}, {0, 1});
    pl.push_back({1, "A"}, {0, 2});

    auto it = pl.sorted_begin();
    auto pr1 = pl.pay(it);
    auto pr2 = pl.pay(it);
    assert(pr1.second == 3);
    assert(pr2.second == 3);
    assert(pl.size() == 3);
}

// 30: liczniki życia – brak wycieków po zniszczeniu playlist
void test_30_lifetime_counters() {
    std::clog << "[30] lifetime counters\n";
    reset_all_counters();
    {
        playlist_t pl;
        pl.push_back({1, "A"}, {10, 1});
        pl.push_back({2, "B"}, {20, 2});
        pl.push_back({1, "A"}, {30, 3});
        assert(TestTrack::live_count > 0);
        assert(TestParams::live_count > 0);
    }
    assert(TestTrack::live_count == 0);
    assert(TestParams::live_count == 0);
}

void test_31_MM_signature_test() {
    std::clog << "[31] MM_test\n";
    reset_all_counters();
    {
        playlist_t pl;
        pl.push_back({1, "A"}, {10, 1});
        pl.push_back({2, "B"}, {20, 2});
        pl.push_back({3, "C"}, {30, 3});
        playlist_t pl_copy(pl);
        auto iB1 = pl_copy.sorted_begin();
        pl.remove({2, "B"});
        ++iB1;
        const auto &track = pl_copy.pay(iB1).first;
        assert(track.name == "B");
    }
}

// ======================== main ========================

int main() {
    try {
        test_01_empty_exceptions();
        test_02_basic_push_front();
        test_03_pop_front_order();
        test_04_clear();
        test_05_remove_all_occurrences();
        test_06_remove_throws_if_missing();
        test_07_play_iterator_sequence();
        test_08_sorted_iterator_unique_sorted();
        test_09_pay_counts();
        test_10_cow_before_write();
        test_11_cow_after_push_back();
        test_12_assignment_and_self_assignment();
        test_13_params_nonconst_detach();
        test_14_params_const_keeps_sharing();
        test_15_push_back_exception_params_copy();
        test_16_remove_exception_compare();
        test_17_play_iterator_compare();
        test_18_sorted_iterator_compare();
        test_19_failed_push_back_keeps_play_iterator();
        test_20_failed_remove_keeps_sorted_iterator();
        test_21_large_n_and_many_copies();
        test_22_cow_and_failed_push_back();
        test_23_const_playlist_usage();
        test_24_assignment_from_temporary();
        test_25_sorted_order_independent_of_insertion();
        test_26_multiple_cow_chain();
        test_27_pop_front_until_empty();
        test_28_mixed_operations();
        test_29_pay_is_read_only();
        test_30_lifetime_counters();
        test_31_MM_signature_test();
        cxx::playlist<int, double> play;
        play.push_back(1, 2.0);
        play.push_back(4, 3.7);
        cxx::playlist<int, double>::play_iterator it = play.play_begin();
        it++;
        assert(play.params(it) == 3.7);
    } catch (...) {
        assert(false && "Uncaught exception in tests");
    }

    std::clog << "ALL STRICT PLAYLIST TESTS PASSED\n";
    return 0;
}
