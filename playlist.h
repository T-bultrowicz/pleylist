#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <cstdlib>
#include <cstddef>
#include <compare>
#include <memory>
#include <map>
#include <vector>
#include <iterator>
#include <list>
#include <stdexcept>

namespace cxx {

    template <typename T, typename P>
    class playlist {
        private:
            ///////////////// DATA TYPES DEFINITIONS /////////////////

            // forward declaration
            struct playNode; 

            // Właściwa playlista, trzyma dane typu playNode
            using p_queue = std::list<playNode>;
            using p_queue_iter = typename p_queue::iterator;

            // Mapa do przechowywania pojedynczych kopi utworów. 
            // Ponadto trzyma listę iteratorów na miejsca w playliście,
            // które odtwarzają utwór spod klucza.
            using track_map = std::map<T, std::list<p_queue_iter>>;

            // Dane o pojedynczym odtworzeniu - trzyma unikatowe dla odtworzenia
            // params. Informację o podanym utworze pozyskujemy z track_nod_ptr.
            // Ponieważ track_nod_ptr w liście będącej wartością trzyma wskaźnik
            // na nas, to przy usuwaniu playNode, naszą odpowiedzialnością jest, aby
            // wywołać na tej liście .erase(self_ptr)
            struct playNode {
                typename track_map::iterator track_nod_ptr;
                typename std::list<p_queue_iter>::iterator self_ptr;
                P params;
            };

            // Tutaj naprawdę przechowywane są dane playlisty, definiowana jest
            // też większośc operacji. Operacje playlisty to głównie wrappery.
            struct playlistData {
                p_queue play_queue{};
                track_map tracks{};

                playlistData() = default;

                // Robi głęboką kopię, zgodną z logiką plejlist (dpowiednio
                // przestawia wskaźniki). Zadba o to by nie stworzyć obiektu,
                // jeśli push_back się nie powiedzie.
                playlistData(const playlistData & other) {
                    const p_queue & pq = other.play_queue;
                    for (auto it = pq.begin(); it != pq.end(); ++it) {
                        push_back(it->track_nod_ptr->first, it->params);
                    }
                }
                playlistData(playlistData && other) = default;
                ~playlistData() = default;

                // Usuwamy operator=, bo pozwalałby na robienie niebezpiecznych
                // kopii, jako, że nasze obiekty trzymają wskaźniki na siebie.
                playlistData & operator=(const playlistData & other) = delete;

                // Tymczasowy jest jednak bezpieczny, bo przywłaszczymy sobie
                // całą strukturę, wraz z odpowiedzialnością za nią.
                playlistData & operator=(playlistData && other) = default;

                // Twoja funkcja - tylko przesunąłem ją niżej, bo tu się też
                // przyda. Ponadto trochę pozmieniałem, by uzyskać strong
                // exception safety.
                void push_back (T const &track, P const &params) {
                    // emplace już gwarantuje strong excp-safety....
                    auto [map_it, added] = tracks.emplace(track, 
                                        std::list<p_queue_iter>{});

                    try {
                        play_queue.push_back({map_it, {}, params});
                    } catch (...) {
                        // rollback zmian 1, push_back nie wyszedł
                        if (added)
                            tracks.erase(map_it);
                        throw;
                    }

                    // get iterator for this node
                    p_queue_iter queue_it = std::prev(play_queue.end());

                    // Może tak jest trochę czytelniej?
                    try {
                        auto list_it = map_it->second.insert
                                            (map_it->second.end(), queue_it);
                        queue_it->self_ptr = list_it;
                    } catch (...) {
                        // rollback zmian 2, jeśli insert nie wyszedł
                        play_queue.pop_back();
                        if (added)
                            tracks.erase(map_it);
                        throw;
                    }
                }

                // if all node destructors are noexept
                // then all the erase / pop function call are also noexept
                // thus this whole fuction should be strongly exeption safe ?  --> I think yep,
                // destructors can't really throw anything.
                void pop_front() {
                    if (play_queue.empty()) {
                        throw std::out_of_range("pop_front, playlist empty");
                    }

                    playNode &node = play_queue.front();

                    node.track_nod_ptr->second.erase(node.self_ptr);
                    // only play of the track => remove it from the map
                    if (node.track_nod_ptr->second.empty()) {
                        tracks.erase(node.track_nod_ptr);
                    }
                    
                    play_queue.pop_front();
                }
            };

            std::shared_ptr<playlistData> data_;
            /* Wydaje mi się że ta flaga jest konieczna, była w jednym z GOTW co
             * Peczarski wrzucał. W implementacji stringów, których kontenery maja
             * się zachowywać 1 do 1 jak nasze playlistData.
             * Ona ma być ustawiana na true, gdy udostępnimy użytkownikowi iterator.
             * Wtedy nie będzie bugu, że damy iterator, skopiujemy siebie, a potem ten
             * iterator zmieni nie tylko nas, ale też obiekt który z siebie stworzyliśmy.
            */
            bool shareable_ = false;

            /*
            Kontener powinien realizować semantykę kopiowania przy modyfikowaniu (ang. copy on write).
            Kopiowanie przy modyfikowaniu to technika optymalizacji szeroko stosowana m.in. w strukturach danych z biblioteki Qt oraz dawniej w implementacjach std::string. Podstawowa jej idea jest taka, że gdy tworzymy kopię obiektu (w C++ za pomocą konstruktora kopiującego lub operatora przypisania), to współdzieli ona wszystkie wewnętrzne zasoby (które mogą być przechowywane w oddzielnym obiekcie na stercie) z obiektem źródłowym.
            Taki stan trwa do momentu, w którym jedna z kopii musi zostać zmodyfikowana.
            Wtedy modyfikowany obiekt tworzy własną kopię zasobów, na których wykonuje modyfikację. Udostępnienie referencji nie-const umożliwiającej modyfikowanie stanu struktury uniemożliwia jej (dalsze) współdzielenie do czasu unieważnienia udzielonej referencji.
            Przyjmujemy, że taka referencja ulega unieważnieniu po dowolnej modyfikacji struktury.
            */
            
            // Po wywołaniu tej funkcji obiekt ma pewność,
            // że brudzi dane, do którychma wyłączny dostęp.
            // Daje strong excp-guarantee, wyrzuca wyjątek dalej.
            void ensure_unique() {
                if (data_.use_count() > 1) {
                    data_ = std::make_shared<playlistData>(*data_);
                }
            }

        public:
            // TODO add const, noexept where its needed
            playlist()
                : data_(std::make_shared<playlistData>()) {}
            
            // Jeśli other jest w trakcie modyfikacji, musimy go skopiować,
            // używamy do tego dobrze przepinającego wskaźniki konstruktora
            // kopiującego klasy playlistData.
            playlist(playlist const &other)
                : data_(!other.shareable_ // if
                    ? std::make_shared<playlistData>(*other.data_) // then
                    : other.data_), shareable_(true) {} // else

            // magia shared_ptr - pozwala nam na użycie default lub proste
            // przypisanie, bez sprawdzania czy this != &opther.
            playlist(playlist &&other) = default;
            ~playlist() = default;

            // Gdy kopia playlistData w nowym std::make_shared się nie uda,
            // warto by rzucić wyjątek (na 90%?), dlatego go nie łapię.
            playlist & operator=(playlist other) {
                data_ = !other.shareable_ // if
                    ? std::make_shared<playlistData>(*other.data_) // then
                    : other.data_; // else
                shareable_ = true;
                return *this;
            }

            // W TYM STYLU, TYLKO ZROBIĆ Z TEGO FUNKCJĘ FUNKCJI
            void push_back (T const &track, P const &params) { // O(log n)
                auto ptr = data_;
                ensure_unique();
                try {
                    data_->push_back(track, params);
                    shareable_ = true;
                } catch (...) {
                    data_= ptr;
                    throw;
                }
            }

            void pop_front() { // O(1) + throws std::out_of_range
                ensure_unique();
                data_->pop_front();
            }

            // O(1) + std::out_of_range
            const std::pair<T const &, P const &> front() const {
                if (data_->play_queue.empty()) {
                    throw std::out_of_range("front, playlist empty");
                }
                
                playNode &node = data_->play_queue.front();
                return {node.track_nod_ptr->first, node.params};
            }

            // O((k + 1)log n) + std::invalid_argument 
            void remove(T const &track) {
                auto map_it = data_->tracks.find(track);
                if (map_it == data_->tracks.end()) {
                    throw std::invalid_argument("remove, unknown track");
                }
                
                ensure_unique();
                
                auto &occurrences = map_it->second;
                for (auto &queue_it : occurrences) {
                    data_->play_queue.erase(queue_it);
                }
                
                data_->tracks.erase(map_it);
            }

            // strong expc guarantee bo tylko make_shared się wywala ->
            // wtedy nie dochodzi do przypisania.
            void clear() {
                data_ = std::make_shared<playlistData>();
            }

            size_t size() const { // O(1)
                return data_->play_queue.size();
            }

            // Implementacja iteratorów
            class play_iterator {
                public:
                    using iterator_category = std::forward_iterator_tag;
                    using value_type = playNode;
                    using difference_type = std::ptrdiff_t;
                    using pointer = p_queue_iter;
                    using reference = value_type&;

                    play_iterator(pointer p = nullptr): ptr{p} {}

                    reference operator*() const noexcept {
                        return *ptr;
                    }

                    pointer operator->() const noexcept {
                        return ptr;
                    }

                    play_iterator & operator++() {
                        ++ptr;
                        return *this;
                    }

                    play_iterator operator++(int) {
                        play_iterator tmp(*this);
                        ++ptr;
                        return tmp;
                    }

                    bool operator==(const play_iterator & oth) const = default;
                    bool operator!=(const play_iterator & oth) const = default;        
                private:
                    pointer ptr;
            };

            class sorted_iterator {
                public:
                    using iterator_category = std::forward_iterator_tag;
                    using value_type = std::pair<T, std::list<p_queue_iter>>;
                    using difference_type = std::ptrdiff_t;
                    using pointer = typename track_map::const_iterator;
                    using reference = const value_type&;

                    sorted_iterator(pointer p = nullptr): ptr{p} {}

                    reference operator*() const noexcept {
                        return *ptr;
                    }

                    pointer operator->() const noexcept {
                        return ptr;
                    }

                    sorted_iterator & operator++() {
                        ++ptr;
                        return *this;
                    }

                    sorted_iterator operator++(int) {
                        sorted_iterator tmp(*this);
                        ++ptr;
                        return tmp;
                    }

                    bool operator==(const sorted_iterator & oth) const = default;
                    bool operator!=(const sorted_iterator & oth) const = default;        
                private:
                    pointer ptr;
            };

            const std::pair<T const &, P const &> play(play_iterator const &it)
            const {
                return {it->track_nod_ptr->first, it->params};
            }

            const std::pair<T const &, size_t> pay(sorted_iterator const &it)
            const {
                return {it->first, it->second.size()};
            }

            P & params(play_iterator const &it) {
                ensure_unique();
                shareable_ = false;

                return it->params;
            }

            const P & params(play_iterator const &it) const {
                return it->params;
            }

            play_iterator play_begin() const {
                return play_iterator(data_->play_queue.begin());
            }

            play_iterator play_end() const {
                return play_iterator(data_->play_queue.end());
            }

            sorted_iterator sorted_begin() const {
                return sorted_iterator(data_->tracks.begin());
            }

            sorted_iterator sorted_end() const {
                return sorted_iterator(data_->tracks.end());
            }
    };

} // namespace cxx

#endif //PLAYLIST_H