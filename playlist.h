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
                std::list<p_queue_iter>::iterator self_ptr;
                P params;
            }

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
        public:
            playlist()
              : data_(std::make_shared<playlistData>()) {}
            
            playlist(playlist const &other);  // O(1)
            playlist(playlist &&other);  // O(1)
            ~playlist(); // O(1)

            // sprawdzać this != &other
            playlist & operator=(playlist other); // O(1)

            void push_back (T const &track, P const &params); // O(log n)
            void pop_front(); // O(1) + throws std::out_of_range
            const std::pair<T const &, P const &> front(); // O(1) + std::out_of_range
            
            void remove(T const &track); // O((k + 1)log n) + std::invalid_argument 
            void clear(); // O(n) - wywala wszystko
            size_t size(); // O(1)

            // iteratorowe
            class play_iterator {
                private:
                    std::list<play>::iterator it;
            }

            class sorted_iterator {
                private:
                    typename track_map::iterator it;
                
            }
            const std::pair<T const &, P const &> play(play_iterator const &it); // O(1)
            const std::pair<T const &, size_t> pay(sorted_iterator const &it); // O(k) !!!
            P & params(play_iterator const &it); // O(1)
            const P & params(play_iterator const &it) const; // O(1)

            play_iterator play_begin(); // O(1)
            play_iterator play_end(); // O(1)
            sorted_iterator sorted_begin(); // O(1)
            sorted_iterator sorted_end(); // O(1)
    };
}

#endif //PLAYLIST_H