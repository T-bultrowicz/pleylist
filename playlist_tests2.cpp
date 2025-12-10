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

// Bardzo prosty typ wyjątku do naszych testów.
struct test_exception : std::exception {
    const char* what() const noexcept override {
        return "test_exception";
    }
};

// Typ T (utwór) z licznikami i możliwością rzucania wyjątków.
struct TestTrack {
    int id{};
    std::string name;

    inline static bool throw_on_copy = false;
    inline static bool throw_on_compare = false;

    inline static int live_count;
    inline static int copy_count;
    inline static int move_count;

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
            throw test_exception{};
        }
        return a.id == b.id;
    }

    friend bool operator!=(TestTrack const& a, TestTrack const& b) {
        return !(a == b);
    }

    friend bool operator<(TestTrack const& a, TestTrack const& b) {
        if (throw_on_compare) {
            throw test_exception{};
        }
        return a.id < b.id;
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
        live_count = 0;
        copy_count = 0;
        move_count = 0;
    }
};


// Typ P (parametry) z możliwością rzucania wyjątków w kopiowaniu.
struct TestParams {
    int volume{};
    int tag{};

    inline static bool throw_on_copy = false;
    inline static int live_count;
    inline static int copy_count;

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
            tag = other.tag;
            ++copy_count;
        }
        return *this;
    }

    ~TestParams() {
        --live_count;
    }

    static void reset_counters() {
        live_count = 0;
        copy_count = 0;
    }
};


using playlist_t = cxx::playlist<TestTrack, TestParams>;
using play_it    = typename playlist_t::play_iterator;
using sorted_it  = typename playlist_t::sorted_iterator;

static void reset_all_counters() {
    TestTrack::reset_counters();
    TestParams::reset_counters();
    TestTrack::throw_on_copy    = false;
    TestTrack::throw_on_compare = false;
    TestParams::throw_on_copy   = false;
}

// Pomocnicze: pobranie adresu T i P z front().
static std::pair<TestTrack const*, TestParams const*>
get_front_ptrs(playlist_t& pl) {
    auto pr = pl.front();
    return { &pr.first, &pr.second };
}

// ========== TESTY ==========

// 1. Podstawowa pusta plejlista, wyjątki front/pop_front.
void test_01_empty_basic() {
    std::clog << "[test_01] empty, front/pop_front exceptions\n";
    reset_all_counters();
    playlist_t pl;
    assert(pl.size() == 0);

    bool thrown = false;
    try {
        (void)pl.front();
    } catch (std::out_of_range const&) {
        thrown = true;
    } catch (...) {
        assert(false);
    }
    assert(thrown);

    thrown = false;
    try {
        pl.pop_front();
    } catch (std::out_of_range const&) {
        thrown = true;
    } catch (...) {
        assert(false);
    }
    assert(thrown);
}

// 2. Podstawowe push_back, front, size, kolejność.
void test_02_push_front_order() {
    std::clog << "[test_02] push_back order & front\n";
    reset_all_counters();
    playlist_t pl;

    TestTrack t1{1, "one"};
    TestTrack t2{2, "two"};
    TestParams p1{10, 100};
    TestParams p2{20, 200};

    pl.push_back(t1, p1);
    pl.push_back(t2, p2);
    assert(pl.size() == 2);

    auto [tp, pp] = get_front_ptrs(pl);
    assert(tp->id == 1);
    assert(pp->volume == 10);
}

// 3. pop_front usuwa poprawnie pierwszy element.
void test_03_pop_front_removal() {
    std::clog << "[test_03] pop_front removal\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "a"}, {1, 1});
    pl.push_back({2, "b"}, {2, 2});
    pl.push_back({3, "c"}, {3, 3});
    assert(pl.size() == 3);

    pl.pop_front();
    assert(pl.size() == 2);

    auto [tp, pp] = get_front_ptrs(pl);
    assert(tp->id == 2);
    assert(pp->volume == 2);
}

// 4. clear usuwa wszystko, begin == end dla obu iteratorów.
void test_04_clear() {
    std::clog << "[test_04] clear()\n";
    reset_all_counters();
    playlist_t pl;
    for (int i = 0; i < 5; ++i) {
        pl.push_back({i, "t"}, {i, i});
    }
    assert(pl.size() == 5);
    pl.clear();
    assert(pl.size() == 0);

    auto itp = pl.play_begin();
    auto etp = pl.play_end();
    assert(itp == etp);

    auto its = pl.sorted_begin();
    auto ets = pl.sorted_end();
    assert(its == ets);
}

// 5. remove usuwa wszystkie wystąpienia danego utworu.
void test_05_remove_all_occurrences() {
    std::clog << "[test_05] remove() all occurrences\n";
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

    // Zostały tylko b.
    auto it = pl.play_begin();
    auto end = pl.play_end();
    for (; it != end; ++it) {
        auto pr = pl.play(it);
        assert(pr.first.id == 2);
    }
}

// 6. remove rzuca invalid_argument jeśli nie ma takiego utworu.
void test_06_remove_throws_if_missing() {
    std::clog << "[test_06] remove() throws on missing track\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {1, 1});
    pl.push_back({2, "B"}, {2, 2});

    bool thrown = false;
    try {
        pl.remove(TestTrack{3, "C"});
    } catch (std::invalid_argument const&) {
        thrown = true;
    } catch (...) {
        assert(false);
    }
    assert(thrown);
}

// 7. play_iterator – poprawna kolejność, pre/post ++.
void test_07_play_iterator_basic() {
    std::clog << "[test_07] play_iterator sequence\n";
    reset_all_counters();
    playlist_t pl;
    for (int i = 0; i < 4; ++i) {
        pl.push_back({i, "T"}, {i, 10 + i});
    }

    auto it = pl.play_begin();
    auto end = pl.play_end();
    int expect = 0;

    // Pre-increment
    while (it != end) {
        auto pr = pl.play(it);
        assert(pr.first.id == expect);
        ++it;
        ++expect;
    }
    assert(expect == 4);

    // Post-increment
    it = pl.play_begin();
    end = pl.play_end();
    expect = 0;
    while (it != end) {
        auto tmp = it++;
        auto pr  = pl.play(tmp);
        assert(pr.first.id == expect);
        ++expect;
    }
    assert(expect == 4);
}

// 8. sorted_iterator zwraca unikalne utwory w porządku T.
void test_08_sorted_iterator_unique_and_sorted() {
    std::clog << "[test_08] sorted_iterator unique + sorted\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({10, "A"}, {0, 0});
    pl.push_back({5,  "B"}, {0, 0});
    pl.push_back({10, "A"}, {0, 0});
    pl.push_back({7,  "C"}, {0, 0});
    pl.push_back({5,  "B"}, {0, 0});

    std::vector<int> ids;
    for (auto it = pl.sorted_begin(); it != pl.sorted_end(); ++it) {
        auto pr = pl.pay(it); // pay zwraca (T, count)
        ids.push_back(pr.first.id);
    }
    // unikalne i posortowane
    assert((ids.size() == 3));
    assert(ids[0] == 5);
    assert(ids[1] == 7);
    assert(ids[2] == 10);
}

// 9. pay zwraca poprawną liczność wystąpień utworu.
void test_09_pay_counts() {
    std::clog << "[test_09] pay() counts occurrences\n";
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

// 10. Kopiowanie plejlisty – współdzielenie danych przed modyfikacją.
void test_10_copy_shares_data_before_write() {
    std::clog << "[test_10] copy shares data before write (COW)\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 1});
    p1.push_back({2, "B"}, {20, 2});

    playlist_t p2 = p1; // copy
    assert(p1.size() == p2.size());

    auto [t1p1, p1p1] = get_front_ptrs(p1);
    auto [t1p2, p1p2] = get_front_ptrs(p2);

    // Brak modyfikacji – powinny współdzielić dane.
    assert(t1p1 == t1p2);
    assert(p1p1 == p1p2);
}

// 11. COW po push_back – modyfikacja jednego obiektu odłącza dane.
void test_11_detach_on_push_back() {
    std::clog << "[test_11] detach on push_back\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 1});

    playlist_t p2 = p1;
    auto [t1p1_before, par1p1_before] = get_front_ptrs(p1);
    auto [t1p2_before, par1p2_before] = get_front_ptrs(p2);
    assert(t1p1_before == t1p2_before);
    assert(par1p1_before == par1p2_before);

    // Modyfikujemy p1 – powinna zajść separacja zasobów.
    p1.push_back({2, "B"}, {20, 2});

    auto [t1p1_after, par1p1_after] = get_front_ptrs(p1);
    auto [t1p2_after, par1p2_after] = get_front_ptrs(p2);

    // Adresy mogą się zmienić w p1 (np. realokacja),
    // ale p1 i p2 powinny już NIE współdzielić frontu.
    assert(!(t1p1_after == t1p2_after && par1p1_after == par1p2_after));
}

// 12. operator= z argumentem przez wartość, w tym self-assignment.
void test_12_assignment_from_value_and_self_assignment() {
    std::clog << "[test_12] operator=(playlist) and self-assignment\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 1});
    p1.push_back({2, "B"}, {20, 2});

    playlist_t p2;
    p2.push_back({3, "C"}, {30, 3});

    // Przypisanie z wartości.
    p2 = p1;
    assert(p2.size() == p1.size());

    // Self-assignment – nie powinno zniszczyć struktury ani przeciekać pamięci.
    p1 = p1;
    assert(p1.size() == 2);
}

// 13. Non-const params() wywołane na jednej kopii powinno odłączyć COW.
void test_13_detach_on_nonconst_params() {
    std::clog << "[test_13] detach on non-const params()\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 1});

    playlist_t p2 = p1;
    auto it1 = p1.play_begin();
    auto it2 = p2.play_begin();

    {
        auto pr1 = p1.play(it1);
        auto pr2 = p2.play(it2);
        assert(&pr1.first == &pr2.first);
        assert(&pr1.second == &pr2.second);
    }

    // Non-const params na p1 – powinno spowodować detach.
    TestParams& ref = p1.params(it1);
    ref.volume = 99;

    auto pr1_after = p1.play(it1);
    auto pr2_after = p2.play(it2);

    // Parametry powinny być różne i pod różnymi adresami.
    assert(pr1_after.second.volume == 99);
    assert(pr2_after.second.volume != 99);
    assert(&pr1_after.second != &pr2_after.second);
}

// 14. Const params() nie powinno odłączać współdzielenia.
void test_14_const_params_does_not_detach() {
    std::clog << "[test_14] const params() does not detach\n";
    reset_all_counters();
    playlist_t p1;
    p1.push_back({1, "A"}, {10, 1});

    playlist_t p2 = p1;

    auto it1 = p1.play_begin();
    auto it2 = p2.play_begin();

    auto const& p1c = static_cast<playlist_t const&>(p1);
    auto const& p2c = static_cast<playlist_t const&>(p2);

    auto& rp1 = p1c.params(it1);
    auto& rp2 = p2c.params(it2);

    assert(&rp1 == &rp2);

    // Po wywołaniu const params nadal powinno współdzielić.
    auto pr1 = p1.play(it1);
    auto pr2 = p2.play(it2);
    assert(&pr1.second == &pr2.second);
}

// 15. push_back: wyjątek w kopiowaniu parametrów nie zmienia stanu plejlisty.
void test_15_push_back_exception_safety_on_params_copy() {
    std::clog << "[test_15] push_back strong guarantee (params copy throws)\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});
    pl.push_back({2, "B"}, {20, 2});

    auto before_size = pl.size();
    auto [t_before, p_before] = get_front_ptrs(pl);

    TestParams throwing_params{30, 3};
    TestParams::throw_on_copy = true;

    bool thrown = false;
    try {
        pl.push_back({3, "C"}, throwing_params);
    } catch (test_exception const&) {
        thrown = true;
    } catch (...) {
        assert(false);
    }
    TestParams::throw_on_copy = false;
    assert(thrown);

    // Stan plejlisty nie powinien się zmienić.
    assert(pl.size() == before_size);
    auto [t_after, p_after] = get_front_ptrs(pl);
    assert(t_before == t_after);
    assert(p_before == p_after);
}

// 16. remove: wyjątek w porównaniu (operator< / ==) nie zmienia stanu plejlisty.
void test_16_remove_exception_safety_on_compare() {
    std::clog << "[test_16] remove strong guarantee (compare throws)\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});
    pl.push_back({2, "B"}, {20, 2});
    pl.push_back({3, "C"}, {30, 3});

    auto before_size = pl.size();

    TestTrack::throw_on_compare = true;
    bool thrown = false;
    try {
        pl.remove(TestTrack{2, "B"});
    } catch (test_exception const&) {
        thrown = true;
    } catch (...) {
        assert(false);
    }
    TestTrack::throw_on_compare = false;
    assert(thrown);

    // Stan plejlisty nie powinien się zmienić.
    assert(pl.size() == before_size);
}

// 17. front() – wyjątek w porównaniu/operacjach nie powinien zmienić stanu.
void test_17_front_exception_transparency() {
    std::clog << "[test_17] front exception transparency\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});

    auto [t_before, p_before] = get_front_ptrs(pl);

    // Sztucznie wywołamy operacje porównania na Track (poza playlistą),
    // a następnie sprawdzimy, że front nadal się zgadza.
    TestTrack::throw_on_compare = true;
    bool thrown = false;
    try {
        (void)(TestTrack{1, "X"} == TestTrack{2, "Y"});
    } catch (test_exception const&) {
        thrown = true;
    } catch (...) {
        assert(false);
    }
    TestTrack::throw_on_compare = false;
    assert(thrown);

    auto [t_after, p_after] = get_front_ptrs(pl);
    assert(t_before == t_after);
    assert(p_before == p_after);
}

// 18. Iteratory play – porównania ==, !=, end.
void test_18_play_iterator_comparisons() {
    std::clog << "[test_18] play_iterator comparisons\n";
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

    // Przejście do końca.
    ++it;
    ++it;
    assert(it == end);
    assert(!(it != end));
}

// 19. Iteratory sorted – porównania i przejście do end.
void test_19_sorted_iterator_comparisons() {
    std::clog << "[test_19] sorted_iterator comparisons\n";
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

    // Dwa kroki do końca (bo 2 unikalne).
    ++it;
    ++it;
    assert(it == end);
}

// 20. Failing push_back nie unieważnia istniejących iteratorów play.
void test_20_failed_push_back_keeps_play_iterators_valid() {
    std::clog << "[test_20] failed push_back keeps play_iterator valid\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});
    pl.push_back({2, "B"}, {20, 2});

    auto it = pl.play_begin();
    auto pr_before = pl.play(it);
    assert(pr_before.first.id == 1);

    // Wymuszenie wyjątku przy kopiowaniu parametrów.
    TestParams::throw_on_copy = true;
    bool thrown = false;
    try {
        pl.push_back({3, "C"}, {30, 3});
    } catch (test_exception const&) {
        thrown = true;
    } catch (...) {
        assert(false);
    }
    TestParams::throw_on_copy = false;
    assert(thrown);

    // Iterator nadal powinien wskazywać na pierwszy element.
    auto pr_after = pl.play(it);
    assert(pr_after.first.id == 1);
    assert(pr_after.second.volume == 10);
}

// 21. Failing remove nie unieważnia iteratorów sorted.
void test_21_failed_remove_keeps_sorted_iterators_valid() {
    std::clog << "[test_21] failed remove keeps sorted_iterator valid\n";
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
    } catch (...) {
        assert(false);
    }
    TestTrack::throw_on_compare = false;
    assert(thrown);

    auto pr_after = pl.pay(it);
    assert(pr_after.first.id == 1);
}

// 22. Kopia wielu plejlist – stres test COW i zarządzania pamięcią.
void test_22_massive_copy_stress() {
    std::clog << "[test_22] massive copy stress\n";
    reset_all_counters();
    playlist_t base;
    for (int i = 0; i < 10; ++i) {
        base.push_back({i, "T"}, {i, i});
    }

    std::vector<playlist_t> vec;
    vec.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        vec.push_back(base);
    }

    for (auto& pl : vec) {
        assert(pl.size() == base.size());
        auto it = pl.play_begin();
        auto pr = pl.play(it);
        assert(pr.first.id == 0);
    }
}

// 23. Kopia const playlist i wywołania const metod.
void test_23_const_playlist_usage() {
    std::clog << "[test_23] const playlist usage\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});
    pl.push_back({2, "B"}, {20, 2});

    playlist_t const& cpl = pl;

    auto it = cpl.play_begin();
    auto end = cpl.play_end();
    int seen = 0;
    while (it != end) {
        auto pr = cpl.play(it);
        (void)pr; // ważne: nie używamy, ale zmuszamy do kompilacji.
        ++it;
        ++seen;
    }
    assert(seen == 2);
}

// 24. Operator= z tymczasowym (copy-and-swap pattern).
playlist_t make_temporary_playlist() {
    playlist_t tmp;
    tmp.push_back({42, "X"}, {1, 2});
    tmp.push_back({43, "Y"}, {3, 4});
    return tmp;
}

void test_24_assignment_from_temporary() {
    std::clog << "[test_24] assignment from temporary\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {10, 1});

    pl = make_temporary_playlist();
    assert(pl.size() == 2);

    auto it = pl.play_begin();
    auto pr = pl.play(it);
    assert(pr.first.id == 42);
}

// 25. Kolejność sorted_iterator niezależna od kolejności wstawiania.
void test_25_sorted_order_independent_of_insertion() {
    std::clog << "[test_25] sorted order independent of insertion\n";
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

// 26. Wielokrotne copy-on-write: kopiowanie łańcuchowe i modyfikacje.
void test_26_multiple_cow_chains() {
    std::clog << "[test_26] multiple COW chains\n";
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

    // Na początku wszystkie współdzielą.
    assert(&pr1.first == &pr2.first && &pr2.first == &pr3.first);
    assert(&pr1.second == &pr2.second && &pr2.second == &pr3.second);

    // Modyfikujemy p2 przez non-const params.
    TestParams& ref2 = p2.params(it2);
    ref2.tag = 77;

    auto pr1a = p1.play(it1);
    auto pr2a = p2.play(it2);
    auto pr3a = p3.play(it3);

    // p2 powinno się odłączyć od p1 i p3.
    assert(&pr2a.second != &pr1a.second);
    assert(&pr2a.second != &pr3a.second);
    assert(&pr1a.second == &pr3a.second);
}

// 27. Usuwanie wszystkich elementów za pomocą pop_front w pętli.
void test_27_pop_front_until_empty() {
    std::clog << "[test_27] pop_front until empty\n";
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
    } catch (...) {
        assert(false);
    }
    assert(thrown);
}

// 28. Test intensywnego mixu push_back, remove, pop_front.
void test_28_mixed_operations() {
    std::clog << "[test_28] mixed operations\n";
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
    pl.pop_front(); // usuwa A
    assert(pl.size() == 3);

    pl.remove(b);   // usuwa B
    assert(pl.size() == 2);

    // Zostały C i A.
    auto it = pl.play_begin();
    auto pr = pl.play(it);
    assert(pr.first.id == 3);
    ++it;
    auto pr2 = pl.play(it);
    assert(pr2.first.id == 1);
}

// 29. Test że pay nie modyfikuje playlisty (liczność stała).
void test_29_pay_is_read_only() {
    std::clog << "[test_29] pay is read-only\n";
    reset_all_counters();
    playlist_t pl;
    pl.push_back({1, "A"}, {0, 0});
    pl.push_back({1, "A"}, {0, 1});
    pl.push_back({1, "A"}, {0, 2});

    auto it = pl.sorted_begin();
    auto pr1 = pl.pay(it);
    assert(pr1.second == 3);

    // Ponowne wywołanie pay powinno dać tę samą liczność.
    auto pr2 = pl.pay(it);
    assert(pr2.second == 3);
    assert(pl.size() == 3);
}

// 30. Liczniki życia – brak wycieków przy prostych scenariuszach.
void test_30_lifetime_counters_basic() {
    std::clog << "[test_30] lifetime counters basic\n";
    reset_all_counters();

    {
        playlist_t pl;
        pl.push_back({1, "A"}, {10, 1});
        pl.push_back({2, "B"}, {20, 2});
        pl.push_back({1, "A"}, {30, 3});

        // Na pewno istnieje jakaś liczba obiektów, ale po zniszczeniu plejlist
        // liczniki powinny wrócić do zera.
        assert(TestTrack::live_count > 0);
        assert(TestParams::live_count > 0);
    }

    assert(TestTrack::live_count == 0);
    assert(TestParams::live_count == 0);
}

// ========= main =========

int main() {
    try {
        test_01_empty_basic();
        test_02_push_front_order();
        test_03_pop_front_removal();
        test_04_clear();
        test_05_remove_all_occurrences();
        test_06_remove_throws_if_missing();
        test_07_play_iterator_basic();
        test_08_sorted_iterator_unique_and_sorted();
        test_09_pay_counts();
        test_10_copy_shares_data_before_write();
        test_11_detach_on_push_back();
        test_12_assignment_from_value_and_self_assignment();
        test_13_detach_on_nonconst_params();
        test_14_const_params_does_not_detach();
        test_15_push_back_exception_safety_on_params_copy();
        test_16_remove_exception_safety_on_compare();
        test_17_front_exception_transparency();
        test_18_play_iterator_comparisons();
        test_19_sorted_iterator_comparisons();
        test_20_failed_push_back_keeps_play_iterators_valid();
        test_21_failed_remove_keeps_sorted_iterators_valid();
        test_22_massive_copy_stress();
        test_23_const_playlist_usage();
        test_24_assignment_from_temporary();
        test_25_sorted_order_independent_of_insertion();
        test_26_multiple_cow_chains();
        test_27_pop_front_until_empty();
        test_28_mixed_operations();
        test_29_pay_is_read_only();
        test_30_lifetime_counters_basic();
    } catch (...) {
        // Jeżeli gdziekolwiek wyciekł wyjątek, testy uznajemy za niezdane.
        assert(false && "Uncaught exception in tests");
    }

    std::clog << "ALL WREDNE TESTS PASSED\n";
    return 0;
}
