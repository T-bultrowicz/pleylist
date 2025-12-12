#include "playlist.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <string>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <functional>

// Makra pomocnicze do testowania
#define ASSERT_THROWS(expr, exType) \
    try { \
        expr; \
        std::cerr << "FAIL: " << #expr << " should have thrown " << #exType << " at line " << __LINE__ << std::endl; \
        std::abort(); \
    } catch (const exType&) { \
        /* OK */ \
    } catch (...) { \
        std::cerr << "FAIL: " << #expr << " threw wrong exception type at line " << __LINE__ << std::endl; \
        std::abort(); \
    }

#define ASSERT_EQ(a, b) \
    if (!((a) == (b))) { \
        std::cerr << "FAIL: " << #a << " != " << #b << " (" << (a) << " vs " << (b) << ") at line " << __LINE__ << std::endl; \
        std::abort(); \
    }

// -----------------------------------------------------------------------------
// Klasy pomocnicze (Mocki)
// -----------------------------------------------------------------------------

// Globalny licznik instancji, aby wykrywać wycieki i zbędne kopie
struct InstanceCounter {
    static int t_params_alive;
    static int t_tracks_alive;
    static int t_copies;
    static bool throw_on_copy;
    
    static void reset() {
        t_params_alive = 0;
        t_tracks_alive = 0;
        t_copies = 0;
        throw_on_copy = false;
    }
};

int InstanceCounter::t_params_alive = 0;
int InstanceCounter::t_tracks_alive = 0;
int InstanceCounter::t_copies = 0;
bool InstanceCounter::throw_on_copy = false;

// Obiekt T: Utwór
class Track {
    int id;
    std::string data;
public:
    Track(int i, std::string d = "") : id(i), data(std::move(d)) {
        InstanceCounter::t_tracks_alive++;
    }
    Track(const Track& other) : id(other.id), data(other.data) {
        if (InstanceCounter::throw_on_copy) {
            throw std::runtime_error("Copy failed induced by test");
        }
        InstanceCounter::t_tracks_alive++;
        InstanceCounter::t_copies++;
    }
    Track(Track&& other) noexcept : id(other.id), data(std::move(other.data)) {
        InstanceCounter::t_tracks_alive++;
        other.id = -1; // moved-from state
    }
    ~Track() {
        InstanceCounter::t_tracks_alive--;
    }
    Track& operator=(const Track& other) {
        if (this != &other) {
            if (InstanceCounter::throw_on_copy) throw std::runtime_error("Copy assign failed");
            id = other.id;
            data = other.data;
        }
        return *this;
    }
    Track& operator=(Track&& other) noexcept {
        id = other.id;
        data = std::move(other.data);
        return *this;
    }

    // Wymagane operatory
    bool operator==(const Track& other) const { return id == other.id; }
    bool operator!=(const Track& other) const { return !(*this == other); }
    bool operator<(const Track& other) const { return id < other.id; }
    
    friend std::ostream& operator<<(std::ostream& os, const Track& t) {
        return os << "T(" << t.id << ")";
    }
};

// Obiekt P: Parametry
class Params {
    
public:
    int val;
    Params(int v = 0) : val(v) { InstanceCounter::t_params_alive++; }
    Params(const Params& other) : val(other.val) {
        InstanceCounter::t_params_alive++;
    }
    ~Params() { InstanceCounter::t_params_alive--; }
    
    bool operator==(const Params& other) const { return val == other.val; }
    friend std::ostream& operator<<(std::ostream& os, const Params& p) {
        return os << "P(" << p.val << ")";
    }
};

using PlaylistT = cxx::playlist<Track, Params>;

// -----------------------------------------------------------------------------
// Testy
// -----------------------------------------------------------------------------

void test_01_basic_push_pop_size() {
    std::cout << "[Test 01] Basic Push/Pop/Size logic" << std::endl;
    PlaylistT p;
    ASSERT_EQ(p.size(), 0);
    p.push_back(Track(1), Params(10));
    p.push_back(Track(2), Params(20));
    ASSERT_EQ(p.size(), 2);
    
    auto f = p.front();
    ASSERT_EQ(f.first, Track(1));
    ASSERT_EQ(f.second, Params(10));
    
    p.pop_front();
    ASSERT_EQ(p.size(), 1);
    ASSERT_EQ(p.front().first, Track(2));
    
    p.pop_front();
    ASSERT_EQ(p.size(), 0);
    ASSERT_THROWS(p.pop_front(), std::out_of_range);
    ASSERT_THROWS(p.front(), std::out_of_range);
}

void test_02_cow_sharing() {
    std::cout << "[Test 02] COW Sharing - no unnecessary copies" << std::endl;
    PlaylistT p1;
    p1.push_back(Track(1), Params(1));
    p1.push_back(Track(2), Params(2));

    InstanceCounter::t_copies = 0; // reset copy counter
    int tracks_before = InstanceCounter::t_tracks_alive;

    PlaylistT p2 = p1; // Copy constructor
    // Powinno współdzielić dane. Liczba "żywych" obiektów Track nie powinna wzrosnąć (chyba że implementacja używa shared_ptr na węzeł, ale Track jest wewnątrz węzła).
    // Jeśli COW jest poprawny, to Tracki nie są kopiowane w tym momencie.
    ASSERT_EQ(InstanceCounter::t_tracks_alive, tracks_before);
    ASSERT_EQ(InstanceCounter::t_copies, 0);

    // Modyfikacja p2 powinna wyzwolić kopiowanie (Deep Copy)
    p2.push_back(Track(3), Params(3)); 
    
    // Teraz p2 ma swoje kopie (1,2) + nowy (3). p1 ma (1,2).
    // Oczekujemy, że wykonano kopie 1 i 2.
    // Uwaga: Dokładna liczba kopii zależy od implementacji (czy kopiujemy wszystko, czy tylko strukturę).
    // Ale na pewno t_tracks_alive musi wzrosnąć o co najmniej długość p1 + 1 (nowy element).
    // Albo dokładniej: p1 ma 2 el, p2 ma 3 el. Razem 5 unikalnych Tracków w pamięci (jeśli pełna separacja).
    // Jeśli implementacja jest bardzo sprytna (np. drzewa funkcyjne), może być mniej, ale typowe COW na std::vector/list/shared_ptr kopiuje całość.
    
    ASSERT_EQ(p1.size(), 2);
    ASSERT_EQ(p2.size(), 3);
    ASSERT_EQ(p1.front().first, Track(1)); 
    // Sprawdź czy p1 jest nienaruszone
    auto it = p1.play_begin();
    ASSERT_EQ(p1.play(it).first, Track(1));
}

void test_03_exception_safety_push() {
    std::cout << "[Test 03] Strong Exception Safety on push_back" << std::endl;
    PlaylistT p;
    p.push_back(Track(1), Params(1));
    
    InstanceCounter::throw_on_copy = true;
    
    // Próba dodania elementu, który rzuci wyjątek przy kopiowaniu do kontenera
    try {
        p.push_back(Track(2), Params(2));
        std::cerr << "Should have thrown!" << std::endl; 
        std::abort();
    } catch (const std::runtime_error&) {
        // OK
    }
    
    InstanceCounter::throw_on_copy = false;
    
    // Stan p nie powinien się zmienić
    ASSERT_EQ(p.size(), 1);
    ASSERT_EQ(p.front().first, Track(1));
}

void test_04_exception_safety_cow() {
    std::cout << "[Test 04] Strong Exception Safety during COW detachment" << std::endl;
    PlaylistT p1;
    for(int i=0; i<10; ++i) p1.push_back(Track(i), Params(i));
    
    PlaylistT p2 = p1; // Shared state
    
    InstanceCounter::throw_on_copy = true;
    
    // Modyfikacja p2 wymaga "odłączenia" (skopiowania 10 elementów).
    // Jeśli w trakcie kopiowania 5. elementu poleci wyjątek, p2 musi pozostać w stanie sprzed modyfikacji
    // (czyli nadal współdzielić z p1 lub być poprawną kopią p1, ale tutaj operacja push ma się nie udać).
    // Ale push_back najpierw robi detach, a potem push. Jeśli detach failuje, to p2 jest const.
    // Zgodnie ze strong guarantee: p2 nie powinno się zmienić widocznie.
    
    try {
        p2.push_back(Track(99), Params(99));
        std::cerr << "Should have thrown during COW detach!" << std::endl;
        std::abort();
    } catch (const std::runtime_error&) {
        // OK
    }
    
    InstanceCounter::throw_on_copy = false;
    
    ASSERT_EQ(p1.size(), 10);
    ASSERT_EQ(p2.size(), 10); // p2 nadal powinno mieć 10 elementów
    
    // Sprawdź czy p2 jest nadal spójne (można czytać)
    int cnt = 0;
    for(auto it = p2.play_begin(); it != p2.play_end(); ++it) {
        p2.play(it);
        cnt++;
    }
    ASSERT_EQ(cnt, 10);
}

void test_05_remove_logic() {
    std::cout << "[Test 05] Remove logic (all occurrences)" << std::endl;
    PlaylistT p;
    p.push_back(Track(1), Params(1));
    p.push_back(Track(2), Params(2));
    p.push_back(Track(1), Params(3));
    p.push_back(Track(3), Params(4));
    p.push_back(Track(1), Params(5));
    
    ASSERT_EQ(p.size(), 5);
    
    p.remove(Track(1));
    
    ASSERT_EQ(p.size(), 2); // Zostały Track(2) i Track(3)
    ASSERT_EQ(p.front().first, Track(2));
    
    // Sprawdź kolejność
    auto it = p.play_begin();
    ASSERT_EQ(p.play(it).first, Track(2));
    ++it;
    ASSERT_EQ(p.play(it).first, Track(3));
}

void test_06_remove_exception() {
    std::cout << "[Test 06] Remove nonexistent throws invalid_argument" << std::endl;
    PlaylistT p;
    p.push_back(Track(1), Params(0));
    
    ASSERT_THROWS(p.remove(Track(99)), std::invalid_argument);
    ASSERT_EQ(p.size(), 1);
}

void test_07_params_const_vs_nonconst() {
    std::cout << "[Test 07] params() non-const triggers COW" << std::endl;
    PlaylistT p1;
    p1.push_back(Track(1), Params(10));
    
    PlaylistT p2 = p1;
    
    // Pobranie const referencji nie powinno rozdzielać
    const PlaylistT& cp2 = p2;
    auto it = cp2.play_begin();
    [[maybe_unused]] const auto& par = cp2.params(it);
    
    // Sprawdzamy adresy w pamięci (hackerska metoda, by sprawdzić współdzielenie,
    // ale w ramach testu czarnej skrzynki sprawdzamy zachowanie).
    // Lepiej sprawdzić liczniki kopii.
    int copies_before = InstanceCounter::t_copies;
    
    // Wywołanie non-const params na p1 powinno wymusić detach, bo użytkownik MOŻE zmodyfikować params.
    // Specyfikacja: "Udostępnienie referencji nie-const... uniemożliwia jej (dalsze) współdzielenie"
    auto it_p1 = p1.play_begin();
    p1.params(it_p1).val = 20; // Zmieniamy w p1
    
    // Jeśli COW zadziałało, p2 powinno mieć stare params
    ASSERT_EQ(cp2.params(it).val, 10);
    ASSERT_EQ(p1.params(p1.play_begin()).val, 20);
    
    // // Jeśli kopia nie nastąpiła, to p2.val byłoby 20 (BŁĄD).
    // // Jeśli nastąpiła, to copies powinno wzrosnąć (dla Tracka wewnątrz, bo deep copy playlisty kopiuje tracki).
    ASSERT_EQ(copies_before < InstanceCounter::t_copies, true);
}

void test_08_sorted_iterator_duplicates() {
    std::cout << "[Test 08] Sorted iterator skips duplicates" << std::endl;
    PlaylistT p;
    p.push_back(Track(5), Params(0));
    p.push_back(Track(1), Params(0));
    p.push_back(Track(5), Params(1));
    p.push_back(Track(2), Params(0));
    p.push_back(Track(1), Params(2));
    
    // Unikalne: 1, 2, 5
    std::vector<int> ids;
    for(auto it = p.sorted_begin(); it != p.sorted_end(); ++it) {
        // Dereferencja iteratora sorted nie jest wprost zdefiniowana w treści zadania jako
        // zwracająca pair czy Track, ale w przykładzie:
        // `pay` przyjmuje sorted_iterator.
        // Ale zazwyczaj iterator ma operator* lub operator->.
        // Jednak specyfikacja `pay(sorted_iterator)` sugeruje, że iterator jest "nieprzezroczysty"
        // lub zwraca coś, co `pay` rozumie.
        // ALE: w treści zadania: "Iterator sorted_iterator umożliwiający przeglądanie utworów... w porządku wyznaczonym przez typ T".
        // Nie ma podanego typu zwracanego przez *it.
        // Jednak w przykładzie używa się `pay(it)`.
        // Zakładamy, że *it zwraca const T& (bo to lista utworów bez powtórzeń).
        // Sprawdźmy `pay`.
        
        // Użyjmy pay, aby sprawdzić co wskazuje iterator.
        auto pair_res = p.pay(it);
        // pair_res to pair<T const &, size_t>
        // T ma operator<<.
        // std::cout << pair_res.first << " x " << pair_res.second << std::endl;
        // Wydobądźmy ID ręcznie, bo T jest prywatne, ale mamy friend ostream
        // Tutaj mamy dostęp do T.
        // Hack: używamy Track::operator< i ==.
        // Zbudujmy wektor kopii.
        ids.push_back(pair_res.first.operator==(Track(1)) ? 1 : 
                      pair_res.first.operator==(Track(2)) ? 2 :
                      pair_res.first.operator==(Track(5)) ? 5 : -1);
    }
    
    ASSERT_EQ(ids.size(), 3);
    ASSERT_EQ(ids[0], 1);
    ASSERT_EQ(ids[1], 2);
    ASSERT_EQ(ids[2], 5);
}

void test_09_pay_counts() {
    std::cout << "[Test 09] Pay method counts correctly" << std::endl;
    PlaylistT p;
    p.push_back(Track(10), Params(1));
    p.push_back(Track(20), Params(2));
    p.push_back(Track(10), Params(3));
    p.push_back(Track(30), Params(4));
    p.push_back(Track(10), Params(5));
    p.push_back(Track(20), Params(6));
    
    // 10: 3 razy
    // 20: 2 razy
    // 30: 1 raz
    
    auto it = p.sorted_begin(); // Powinno być 10
    auto res1 = p.pay(it);      // Używamy res1
    ASSERT_EQ(res1.first, Track(10));
    ASSERT_EQ(res1.second, 3);
    
    ++it; // 20
    auto res2 = p.pay(it);      // Tworzymy nową zmienną res2
    ASSERT_EQ(res2.first, Track(20));
    ASSERT_EQ(res2.second, 2);
    
    ++it; // 30
    auto res3 = p.pay(it);      // Tworzymy nową zmienną res3
    ASSERT_EQ(res3.first, Track(30));
    ASSERT_EQ(res3.second, 1);
}


void test_10_pop_front_invalidation() {
    std::cout << "[Test 10] Pop front maintains iterators to other elements (if sequence impl allows)" << std::endl;
    // Specyfikacja: "operacje modyfikujące zakończone niepowodzeniem nie mogą unieważniać iteratorów."
    // A zakończone powodzeniem?
    // Dla vectora pop_front (erase(begin)) unieważnia wszystko.
    // Dla listy/deque tylko usuwany element.
    // Ponieważ wymagane jest pop_front O(1), sugeruje to listę lub deque.
    // Sprawdźmy, czy iterator do drugiego elementu przeżyje usunięcie pierwszego.
    
    PlaylistT p;
    p.push_back(Track(1), Params(1));
    p.push_back(Track(2), Params(2));
    
    auto it1 = p.play_begin();
    auto it2 = it1; ++it2; // wskazuje na Track(2)
    
    p.pop_front(); // usuwa Track(1)
    
    // it1 jest unieważniony (dangling), nie używamy go.
    // it2 powinien być ważny i wskazywać na nowy początek.
    
    ASSERT_EQ(p.play(it2).first, Track(2));
        ASSERT_EQ(it2 == p.play_begin(), true);
}

void test_11_clear_complexity_and_memory() {
    std::cout << "[Test 11] Clear clears memory" << std::endl;
    PlaylistT p;
    for(int i=0; i<100; ++i) p.push_back(Track(i), Params(i));
    
    int tracks_before = InstanceCounter::t_tracks_alive;
    ASSERT_EQ(tracks_before >= 100, true);
    
    p.clear();
    
    ASSERT_EQ(p.size(), 0);
    // Liczba tracków powinna spaść (zakładając brak innych kopii)
    ASSERT_EQ(InstanceCounter::t_tracks_alive < tracks_before, true);
    if (InstanceCounter::t_tracks_alive != 0) {
        // Może być 0, chyba że coś innego trzyma (np. global variables, ale tu resetujemy).
        // W tym teście t_tracks_alive powinno wrócić do 0, jeśli testujemy tylko to.
        // Ale mamy mocki w innych funkcjach? Nie, Track jest lokalny w scope, ale mamy statyczny licznik.
        // Ten test uruchamiamy w main, gdzie inne obiekty mogły umrzeć.
    }
}

void test_12_assignment_operator_strong() {
    std::cout << "[Test 12] Assignment operator Strong Guarantee" << std::endl;
    PlaylistT p1;
    p1.push_back(Track(1), Params(1));
    
    PlaylistT p2;
    p2.push_back(Track(2), Params(2));
    
    // Wymuszamy błąd podczas przypisania?
    // Trudno wymusić błąd w operator= dla shared_ptr (to tylko atomowy inkrement).
    // Ale jeśli implementacja robi Deep Copy od razu (nie COW), to można.
    // W COW przypisanie jest noexcept (zazwyczaj).
    // Jeśli p2 = p1 rzuca, to p2 nie powinno się zmienić.
    
    // Test: Samo przypisanie działa i współdzieli.
    p2 = p1;
    ASSERT_EQ(p2.size(), 1);
    ASSERT_EQ(p2.front().first, Track(1));
    
    // Modyfikacja p1 nie zmienia p2 (COW check again)
    p1.pop_front();
    ASSERT_EQ(p1.size(), 0);
    ASSERT_EQ(p2.size(), 1);
}

void test_13_move_semantics() {
    std::cout << "[Test 13] Move semantics leaves source empty" << std::endl;
    PlaylistT p1;
    p1.push_back(Track(1), Params(1));
    
    PlaylistT p2(std::move(p1));
    
    ASSERT_EQ(p2.size(), 1);
    // p1 powinno być puste
    ASSERT_EQ(p1.size(), 0); 
    
    // Użycie p1 powinno być bezpieczne (np. dodanie)
    p1.push_back(Track(2), Params(2));
    ASSERT_EQ(p1.size(), 1);
}

void test_14_iterator_comparison() {
    std::cout << "[Test 14] Iterator comparison and traversal" << std::endl;
    PlaylistT p;
    p.push_back(Track(1), Params(1));
    
    auto it = p.play_begin();
    auto end = p.play_end();
    
    ASSERT_EQ(it != end, true);
    ASSERT_EQ(it == end, false);
    
    auto it2 = it;
    ASSERT_EQ(it == it2, true);
    
    ++it;
    ASSERT_EQ(it == end, true);
    
    // Postinkrementacja
    it = p.play_begin();
    auto prev = it++;
    ASSERT_EQ(prev ==  p.play_begin(), true);
    ASSERT_EQ(it == end, true);
}

void test_15_self_assignment() {
    std::cout << "[Test 15] Self assignment is safe" << std::endl;
    PlaylistT p;
    p.push_back(Track(1), Params(1));
    
    p = p; // Clang może ostrzegać, ale to legalny C++.
    
    ASSERT_EQ(p.size(), 1);
    ASSERT_EQ(p.front().first, Track(1));
}

void test_16_insert_large_params() {
    std::cout << "[Test 16] Large params copying" << std::endl;
    // Sprawdzamy czy P jest kopiowane poprawnie przy push
    PlaylistT p;
    Params par(999);
    p.push_back(Track(1), par);
    
    ASSERT_EQ(p.front().second, Params(999));
}

void test_17_remove_updates_sorted_iterators() {
    std::cout << "[Test 17] Remove logic updates sorted view" << std::endl;
    PlaylistT p;
    p.push_back(Track(1), Params(0));
    p.push_back(Track(2), Params(0));
    
    // Mamy 1 i 2.
    // Iterujemy sorted.
    auto it = p.sorted_begin();
    // Zakładamy kolejność 1, 2.
    ASSERT_EQ(p.pay(it).first, Track(1));
    
    p.remove(Track(1));
    // Teraz sorted powinno zaczynać się od 2.
    
    auto it2 = p.sorted_begin();
    ASSERT_EQ(p.pay(it2).first, Track(2));
    
    ASSERT_EQ(p.sorted_begin() != p.sorted_end(), true);
    auto it3 = p.sorted_begin(); ++it3;
    ASSERT_EQ(it3 == p.sorted_end(), true);
}

void test_18_params_reference_stability() {
    std::cout << "[Test 18] Params reference stability until invalidation" << std::endl;
    // "Udostępnienie referencji nie-const... uniemożliwia jej (dalsze) współdzielenie do czasu unieważnienia udzielonej referencji."
    // Oznacza to, że po pobraniu referencji, obiekt jest "locked" jako unique? 
    // Albo po prostu zrobiliśmy detach i mamy swój egzemplarz.
    
    PlaylistT p;
    p.push_back(Track(1), Params(10));
    
    auto it = p.play_begin();
    Params& ref = p.params(it);
    
    ref.val = 20;
    
    ASSERT_EQ(p.front().second.val, 20); // Zmiana widoczna
    
    // Dodanie elementu może spowodować relokację (jeśli wektor), ale w zadaniu pop_front O(1) 
    // sugeruje listę/deque (deque nie unieważnia referencji przy push_back/front, lista też nie).
    // Jeśli lista, referencja powinna być ważna.
    
    p.push_back(Track(2), Params(30));
    
    // ref powinno nadal działać (dla std::list/deque to prawda).
    // Jeśli implementacja to vector, to to może być UB, ale wymaganie pop_front O(1) wyklucza vector (chyba że circular buffer).
    // Zakładamy node-based container lub deque.
    ref.val = 25;
    ASSERT_EQ(p.front().second.val, 25);
}

void test_19_sorted_iterator_constness() {
    std::cout << "[Test 19] Sorted iterator behaves like const_iterator" << std::endl;
    PlaylistT p;
    p.push_back(Track(1), Params(0));
    
    auto it = p.sorted_begin();
    (void) it;
    // Nie da się zmodyfikować przez sorted_iterator (kompilator by zabronił, tu nie sprawdzimy tego w runtime).
    // Ale możemy sprawdzić czy działa na const obiekcie.
    const PlaylistT cp = p;
    auto cit = cp.sorted_begin();
    ASSERT_EQ(cit == cp.sorted_end(), false);
}

void test_20_remove_current_play_iterator() {
    std::cout << "[Test 20] Remove element pointed by play_iterator" << std::endl;
    PlaylistT p;
    p.push_back(Track(1), Params(0));
    p.push_back(Track(2), Params(0));
    
    auto it = p.play_begin(); // Track(1)
    (void) it;

    p.remove(Track(1));
    
    // it staje się dangling. Nie możemy go użyć.
    // Ale play_begin() powinno wskazać na 2.
    ASSERT_EQ(p.play(p.play_begin()).first, Track(2));
}

void test_21_transitivity_of_cow() {
    std::cout << "[Test 21] Transitivity of COW (A=B=C)" << std::endl;
    PlaylistT p1;
    p1.push_back(Track(1), Params(0));
    
    PlaylistT p2 = p1;
    PlaylistT p3 = p2;
    
    // Wszystkie 3 dzielą te same dane.
    InstanceCounter::t_copies = 0;
    
    p2.push_back(Track(2), Params(0));
    
    // p2 się oderwało. p1 i p3 powinny nadal współdzielić dane między sobą!
    // Sprawdzenie: modyfikacja p1 powinna pociągnąć kopię dla p1, zostawiając p3.
    // Lub: p1 i p3 są const-safe.
    
    // Jeśli p2 się oderwało, zrobiło kopię.
    // p1 i p3 wskazują na stare dane.
    ASSERT_EQ(p1.size(), 1);
    ASSERT_EQ(p3.size(), 1);
    ASSERT_EQ(p2.size(), 2);
    
    // Czy p1 i p3 nadal współdzielą?
    // Jeśli tak, t_copies nie powinno wzrosnąć przy dostępie do czytania.
    // Zróbmy modyfikację p1.
    int copies_snapshot = InstanceCounter::t_copies;
    p1.push_back(Track(3), Params(0));
    (void) copies_snapshot;

    // Teraz p1 też się oderwało (od p3).
    ASSERT_EQ(p1.size(), 2);
    ASSERT_EQ(p3.size(), 1);
    
    // p3 powinno nadal mieć Track(1).
    ASSERT_EQ(p3.front().first, Track(1));
}

void test_22_empty_playlist_iterators() {
    std::cout << "[Test 22] Empty playlist iterators equality" << std::endl;
    PlaylistT p;
    ASSERT_EQ(p.play_begin() == p.play_end(), true);
    ASSERT_EQ(p.sorted_begin() == p.sorted_end(), true);
}

void test_23_pay_complexity_simulation() {
    std::cout << "[Test 23] Pay complexity (linear in k) check logic" << std::endl;
    // Nie sprawdzimy czasu, ale sprawdzimy poprawność dla dużej liczby duplikatów.
    PlaylistT p;
    for(int i=0; i<100; ++i) {
        p.push_back(Track(1), Params(i));
    }
    // Track(1) występuje 100 razy.
    auto it = p.sorted_begin();
    auto res = p.pay(it);
    ASSERT_EQ(res.first, Track(1));
    ASSERT_EQ(res.second, 100);
}

void test_24_copy_params_on_get() {
    std::cout << "[Test 24] Params non-const get returns reference to internal node" << std::endl;
    // Sprawdzenie czy params zwraca referencję do wewnętrznej struktury, a nie kopię tymczasową.
    PlaylistT p;
    p.push_back(Track(1), Params(10));
    auto it = p.play_begin();
    p.params(it).val = 999;
    
    ASSERT_EQ(p.front().second.val, 999);
}

void test_25_nasty_types() {
    std::cout << "[Test 25] Type T with specific comparison logic" << std::endl;
    // Track porównuje tylko po ID.
    // Dodajmy Track(1, "A") i Track(1, "B").
    // Traktowane jako ten sam utwór (równość T).
    // Playlist przechowuje tylko jedną kopię T?
    // "Należy przechowywać tylko po jednej kopii takiego samego utworu".
    // To oznacza, że jeśli wstawiam Track(1, "B"), a jest już Track(1, "A"), to powinno użyć istniejącego T (czyli "A").
    
    PlaylistT p;
    p.push_back(Track(1, "Original"), Params(1));
    p.push_back(Track(1, "Duplicate"), Params(2));
    
    ASSERT_EQ(p.size(), 2); // 2 odtworzenia
    
    // Sprawdźmy co siedzi w środku.
    auto it = p.play_begin();
    Track t1 = p.play(it).first; // kopia
    ++it;
    Track t2 = p.play(it).first;
    
    // Obie powinny mieć "Original" w data, jeśli implementacja deduplikuje T.
    // Ponieważ Track(1, "Original") == Track(1, "Duplicate") (bo id takie samo).
    // Wymóg: "Należy przechowywać tylko po jednej kopii takiego samego utworu".
    
    // Musimy sprawdzić prywatne pole data w Track? Nie mamy dostępu.
    // Ale Track(1, "Original") == Track(1, "Duplicate").
    // Nie rozróżnimy ich operatorem ==.
    // Ale możemy sprawdzić adresy, jeśli play zwraca const ref.
    
    const Track& ref1 = p.play(p.play_begin()).first;
    const Track& ref2 = p.play(++p.play_begin()).first;
    
    ASSERT_EQ(&ref1, &ref2); // Powinny wskazywać na ten sam obiekt w pamięci!
}


int main() {
    try {
        InstanceCounter::reset();
        
        test_01_basic_push_pop_size();
        InstanceCounter::reset();
        
        test_02_cow_sharing();
        InstanceCounter::reset();
        
        test_03_exception_safety_push();
        InstanceCounter::reset();
        
        test_04_exception_safety_cow();
        InstanceCounter::reset();
        
        test_05_remove_logic();
        InstanceCounter::reset();
        
        test_06_remove_exception();
        InstanceCounter::reset();
        
        test_07_params_const_vs_nonconst();
        InstanceCounter::reset();
        
        test_08_sorted_iterator_duplicates();
        InstanceCounter::reset();
        
        test_09_pay_counts();
        InstanceCounter::reset();
        
        test_10_pop_front_invalidation();
        InstanceCounter::reset();
        
        test_11_clear_complexity_and_memory();
        InstanceCounter::reset();
        
        test_12_assignment_operator_strong();
        InstanceCounter::reset();
        
        test_13_move_semantics();
        InstanceCounter::reset();
        
        test_14_iterator_comparison();
        InstanceCounter::reset();
        
        test_15_self_assignment();
        InstanceCounter::reset();
        
        test_16_insert_large_params();
        InstanceCounter::reset();
        
        test_17_remove_updates_sorted_iterators();
        InstanceCounter::reset();
        
        test_18_params_reference_stability();
        InstanceCounter::reset();
        
        test_19_sorted_iterator_constness();
        InstanceCounter::reset();
        
        test_20_remove_current_play_iterator();
        InstanceCounter::reset();
        
        test_21_transitivity_of_cow();
        InstanceCounter::reset();
        
        test_22_empty_playlist_iterators();
        InstanceCounter::reset();
        
        test_23_pay_complexity_simulation();
        InstanceCounter::reset();
        
        test_24_copy_params_on_get();
        InstanceCounter::reset();
        
        test_25_nasty_types();
        InstanceCounter::reset();
        
        std::cout << "---------------------------------------------------" << std::endl;
        std::cout << "ALL TESTS PASSED" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Uncaught exception in main: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}