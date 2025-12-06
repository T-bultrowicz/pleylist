#include "playlist.h"

#ifdef NDEBUG
  #undef NDEBUG
#endif

#include <cassert>
#include <cstddef>
#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>
#include <string_view>

using std::advance;
using std::array;
using std::as_const;
using std::clog;
using std::invalid_argument;
using std::ostream;
using std::out_of_range;
using std::pair;
using std::string_view;
using std::size_t;
using std::vector;

using cxx::playlist;

namespace {
  [[maybe_unused]] const unsigned BIG_VALUE = 100000;

  // Plejlista pewnego radia zawiera nazwy utworów oraz liczone od początku
  // utworu czasy w sekundach rozpoczęcia i zakończenia odtwarzania.
  using params_t = pair<unsigned, unsigned>;
  using radio_t = playlist<string_view, params_t>;
  using play_return_t = std::pair<string_view const &, params_t const &>;
  using pay_return_t = std::pair<string_view const &, size_t>;

  [[maybe_unused]] ostream & operator<<(ostream &os, play_return_t const &p) {
    os << p.first << ' ' << p.second.first << ':' << p.second.second << '\n';
    return os;
  }

  [[maybe_unused]] ostream & operator<<(ostream &os, pay_return_t const &p) {
    os << p.first << ' ' << p.second << '\n';
    return os;
  }

  [[maybe_unused]] array<string_view, 4> tracks = {"zerowe", "pierwsze",
                                                   "drugie", "trzecie"};
  [[maybe_unused]] array<params_t, 7> params = {{{0, 0}, {0, 1}, {0, 2}, {0, 3},
                                                 {0, 4}, {0, 5}, {0, 6}}};

  // Symulujemy odtwarzanie plajlisty.
  template<typename T, typename P>
  void play(playlist<T, P> const &pl) {
    using pit_t = playlist<T, P>::play_iterator;
    for (pit_t pit = pl.play_begin(); pit != pl.play_end(); ++pit)
      clog << pl.play(pit);
  }

  // Plejlistę można też odtwarzać w inny sposób.
  template<typename T, typename P>
  void lay(playlist<T, P> &pl) {
    while (pl.size() > 0) {
      clog << pl.front();
      pl.pop_front();
    }
  }

  // Wypisujemy, ile komu zapłacić za odtwarzanie jego utworu.
  template<typename T, typename P>
  void pay(playlist<T, P> const &pl) {
    typename playlist<T, P>::sorted_iterator pit = pl.sorted_begin();
    while (pit != pl.sorted_end())
      clog << pl.pay(pit++);
  }
}

int main() {
  radio_t playlist1;

  assert(playlist1.size() == 0);

  for (size_t i = 0; i < tracks.size(); ++i)
     playlist1.push_back(tracks[i], params[i]);
  playlist1.push_back(tracks[1], params[4]);
  playlist1.push_back(tracks[1], params[5]);
  playlist1.push_back(tracks[0], params[6]);

  assert(playlist1.size() == tracks.size() + 3);

  clog << "# Odtwarzamy pierwszy raz.\n";
  play(playlist1);
  clog << "# Płacimy.\n";
  pay(playlist1);
  clog << "# Odtwarzamy drugi raz, usuwając utwory.\n";
  lay(playlist1);

  assert(playlist1.size() == 0);

  clog << "# Testujemy zgłaszanie wyjątków.\n";
  bool catched = false;
  try {
    [[maybe_unused]] auto r = playlist1.front();
    assert(0);
  }
  catch (out_of_range const &exception) {
    clog << exception.what() << '\n';
    catched = true;
  }
  catch (...) {
    assert(0);
  }
  assert(catched);

  catched = false;
  try {
    playlist1.pop_front();
    assert(0);
  }
  catch (out_of_range const &exception) {
    clog << exception.what() << '\n';
    catched = true;
  }
  catch (...) {
    assert(0);
  }
  assert(catched);

  catched = false;
  try {
    playlist1.remove(tracks[0]);
    assert(0);
  }
  catch (invalid_argument const &exception) {
    clog << exception.what() << '\n';
    catched = true;
  }
  catch (...) {
    assert(0);
  }
  assert(catched);

  clog << "# Dodajemy utwory i odtwarzamy trzy początkowe.\n";
  playlist1.push_back(tracks[3], params[0]);
  playlist1.push_back(tracks[2], params[1]);
  playlist1.push_back(tracks[3], params[2]);
  playlist1.push_back(tracks[2], params[3]);
  playlist1.push_back(tracks[1], params[4]);
  auto it1 = playlist1.play_begin();
  clog << playlist1.play(it1++);
  clog << playlist1.play(it1++);
  clog << playlist1.play(it1);

  clog << "# Zmieniamy parametry i odtwarzamy całość.\n";
  auto &p1 = playlist1.params(it1);
  p1 = {17, 52};
  play(playlist1);

  [[maybe_unused]] auto &p2 = as_const(playlist1).params(it1);
  // p2 = {121, 177}; // Nie kompiluje się.

  clog << "# Musimy zapłacić.\n";
  auto it2 = playlist1.sorted_begin();
  clog << playlist1.pay(++it2);
  clog << playlist1.pay(++it2);

  clog << "# Usuwamy jeden utwór i odtwarzamy.\n";
  playlist1.remove(tracks[3]);
  play(playlist1);

  clog << "# Płacimy za ostatnie odtworzenia.\n";
  pay(playlist1);

  radio_t playlist2;
  vector<radio_t> vec;
  for (unsigned i = 0; i < BIG_VALUE; i++)
    playlist2.push_back(tracks[0], {0, i});
  assert(playlist2.size() == BIG_VALUE);
  for (unsigned i = 0; i < 10 * BIG_VALUE; i++)
    vec.push_back(playlist2); // Wszystkie obiekty w vec współdzielą dane.
}
