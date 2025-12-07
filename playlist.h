#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <cstdlib>
#include <cinttypes>
#include <memory>
#include <map>
#include <vector>
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
            // params. Informację o podanym utworze pozyskujemy z trackNode_ptr.
            // Ponieważ trackNode_ptr w liście będącej wartością trzyma wskaźnik
            // na nas, to przy usuwaniu playNode, naszą odpowiedzialnością jest, aby
            // wywołać na tej liście .erase(self_ptr)
            struct playNode {
                typename track_map::iterator trackNode_ptr;
                typename std::list<p_queue_iter>::iterator self_ptr;
                P params;
            };

            // Tutaj naprawdę przechowywane są dane playlisty, definiowana jest
            // też większośc operacji. Operacje playlisty to głównie wrappery.
            struct playlistData {
                p_queue play_queue;
                track_map tracks;


                playlistData() = default;
                playlistData & operator=(const playlistData & other) = delete;
            };

            // Dane playlisty -> shared_ptr na dane, oraz flaga,
            // informująca czy wnętrze playlisty może być współdzielone.
            std::shared_ptr<playlistData> data_;
            bool unshareable_ = false;

            /*
            Kontener powinien realizować semantykę kopiowania przy modyfikowaniu (ang. copy on write).
            Kopiowanie przy modyfikowaniu to technika optymalizacji szeroko stosowana m.in. w strukturach danych z biblioteki Qt oraz dawniej w implementacjach std::string. Podstawowa jej idea jest taka, że gdy tworzymy kopię obiektu (w C++ za pomocą konstruktora kopiującego lub operatora przypisania), to współdzieli ona wszystkie wewnętrzne zasoby (które mogą być przechowywane w oddzielnym obiekcie na stercie) z obiektem źródłowym.
            Taki stan trwa do momentu, w którym jedna z kopii musi zostać zmodyfikowana.
            Wtedy modyfikowany obiekt tworzy własną kopię zasobów, na których wykonuje modyfikację. Udostępnienie referencji nie-const umożliwiającej modyfikowanie stanu struktury uniemożliwia jej (dalsze) współdzielenie do czasu unieważnienia udzielonej referencji.
            Przyjmujemy, że taka referencja ulega unieważnieniu po dowolnej modyfikacji struktury.
            */
            // wydaje mi sić że tak to można rozwiazać:
            void ensure_unique() {
                if (data_.use_count() > 1) {
                    // TODO: głęboka kopia danych z data_ do siebie :P
                    // można chyba zrobić kilka konstruktorów do playlistData
                    // i to zaimplementować za pomocą jednego z nich
                }
            }

        public:
            // TODO add const, noexept where its needed
            playlist()
                : data_(std::make_shared<playlistData>()) {}
            
            playlist(playlist const &other);  // O(1)
            playlist(playlist &&other);  // O(1)
            ~playlist(); // O(1)

            // sprawdzać this != &other
            playlist & operator=(playlist other); // O(1)

            void push_back (T const &track, P const &params) { // O(log n)
                ensure_unique();
                auto map_it = data_->tracks.find(track);
                // add if its a new track
                if (map_it == data_->tracks.end()) {
                    // emplace().first is an iteratior to the inserted element
                    map_it = data_->tracks.emplace(
                        track, std::list<p_queue_iter>{}).first;
                }

                // add playNode in the queue with a temporary nullptr
                data_->play_queue.push_back({map_it, nullptr, params});
                // get iterator for this node
                p_queue_iter queue_it = std::prev(data_->play_queue.end());

                // add this iterator to the back of the list in the map, using
                // insert to get an iterator back,
                // then add said iterator to our node in queue, 
                // replacing the temporary nullptr.

                // TODO: phew that was a long comment but the line is confusing so ... mby this coudld be refactored using some tmp variable like "list_it", but idk
                queue_it->self_ptr = map_it->second.insert(map_it->second.end(),
                                                           queue_it);
            }

            void pop_front() { // O(1) + throws std::out_of_range
                if (data_->play_queue.empty()) {
                    throw std::out_of_range("pop_front(), empty playlist");
                }
                ensure_unique();

                playNode &node = data_->play_queue.front();
                node.trackNode_ptr->second.erase(node.self_ptr);
                data_->play_queue.pop_front();
            }

            // O(1) + std::out_of_range
            const std::pair<T const &, P const &> front() {
                if (data_->play_queue.empty()) {
                    throw std::out_of_range("front(), empty playlist");
                }
                
                playNode &node = data_->play_queue.front();
                return {node.trackNode_ptr->first, node.params};
            }

            // O((k + 1)log n) + std::invalid_argument 
            void remove(T const &track) {
                auto map_it = data_->tracks.find(track);
                if (map_it == data_->tracks.end()) {
                    throw std::invalid_argument("remove(), unknown track");
                }
                
                ensure_unique();
                
                auto &occurrences = map_it->second;
                for (auto &queue_it : occurrences) {
                    data_->play_queue.erase(queue_it);
                }
                
                data_->tracks.erase(map_it);
            }

            void clear(); // O(n) - wywala wszystko

            size_t size() { // O(1)
                return data_->play_queue.size();
            }

            // iteratorowe
            class play_iterator {
                private:
                    // std::list<playN>::iterator it;
            };

            class sorted_iterator {
                private:
                    typename track_map::iterator it;
                
            };
            const std::pair<T const &, P const &> play(play_iterator const &it); // O(1)
            const std::pair<T const &, size_t> pay(sorted_iterator const &it); // O(k) !!!
            P & params(play_iterator const &it); // O(1)
            const P & params(play_iterator const &it) const; // O(1)

            play_iterator play_begin(); // O(1)
            play_iterator play_end(); // O(1)
            sorted_iterator sorted_begin(); // O(1)
            sorted_iterator sorted_end(); // O(1)
    };

} // namespace cxx

#endif //PLAYLIST_H