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

            // Forward declaration
            struct playNode; 

            // Actual playlist, holds playNodes
            using p_queue = std::list<playNode>;
            using p_queue_iter = typename p_queue::iterator;

            // Map that holds singular copies of tracks. Besides that
            // it holds list of iters to positions where track is played.
            using track_map = std::map<T, std::list<p_queue_iter>>;

            /* Data about singular play, holds unique params for that play.
             * Has track_nod_ptr to 'download' track from a track_map. Self_ptr,
             * is used only when deleting, to quickly erase info that track_map
             * holds, pointing at this-to-be-deleted node.
             */
            struct playNode {
                typename track_map::iterator track_nod_ptr;
                std::list<p_queue_iter>::iterator self_ptr;
                P params;
            };

            // Here actual playlist data is stored. It provides save deep
            // copy-constructor that rebuilds pointer structure.
            struct playlistData {
                p_queue play_queue{};
                track_map tracks{};

                playlistData() = default;
                playlistData(const playlistData & other) {
                    const p_queue & pq = other.play_queue;
                    for (auto it = pq.begin(); it != pq.end(); ++it) {
                        push_back(it->track_nod_ptr->first, it->params);
                    }
                }
                playlistData(playlistData && other) = default;
                ~playlistData() = default;

                // Not really used, but very dangerous when used defaultly,
                // (not like copy constructor) so better left deleted.
                playlistData & operator=(const playlistData & other) = delete;

                /* Functions that correctly handles pointer-pinning when adding
                 * a single play to a playlist. Guarantees nothing is added / 
                 * changed when exception is thrown.
                 */
                void push_back (T const &track, P const &params) {
                    // emplace już gwarantuje strong excp-safety....
                    auto [map_it, added] = tracks.emplace(track, 
                                        std::list<p_queue_iter>{});

                    try {
                        play_queue.push_back({map_it, {}, params});
                    } catch (...) {
                        // rollback 1, push_back failed
                        if (added)
                            tracks.erase(map_it);
                        throw;
                    }

                    // get iterator for this node
                    p_queue_iter queue_it = std::prev(play_queue.end());

                    try {
                        auto list_it = map_it->second.insert
                                            (map_it->second.end(), queue_it);
                        queue_it->self_ptr = list_it;
                    } catch (...) {
                        // rollback 2, insert failed
                        play_queue.pop_back();
                        if (added)
                            tracks.erase(map_it);
                        throw;
                    }
                }
            };

            /* Flag and shared_ptr needed to provide COW for playlist object.
             * Shareable is set to true whenever we give to user modifying
             * reference, and set to false after aby other sort of modifying
             * operation. Flag sets COW optimisation off.
             */
            std::shared_ptr<playlistData> data_;
            bool shareable_ = true;

            // Makes data_ point at a new copy, when data is shared by more
            // than a [count] pointer instances. Helper function.
            void ensure_count(long int count) {
                if (data_.use_count() > count) {
                    data_ = std::make_shared<playlistData>(*data_);
                }
            }

        public:
            playlist()
                : data_(std::make_shared<playlistData>()) {}

            playlist(playlist const &other)
                : data_(!other.shareable_                          // if
                    ? std::make_shared<playlistData>(*other.data_) // then
                    : other.data_), shareable_(true) {}            // else

            // Although technically we can leave other in damaged state, we
            // leave him in correct, empty state.
            playlist(playlist &&other)
                : data_(std::move(other.data_)), shareable_(other.shareable_) {
                    other.data_ = std::make_shared<playlistData>();
                    other.shareable_ = true;
                }
            
            ~playlist() = default;
            playlist & operator=(playlist other) {
                data_ = !other.shareable_                          // if
                    ? std::make_shared<playlistData>(*other.data_) // then
                    : other.data_;                                 // else
                shareable_ = true;
                return *this;
            }

            // Uses push_back inside playlistData class.
            void push_back (T const &track, P const &params) { // O(log n)
                auto ptr = data_;
                try {
                    ensure_count(2);
                    data_->push_back(track, params);
                    shareable_ = true;
                } catch (...) {
                    data_= ptr;
                    throw;
                }
            }

            void pop_front() {
                if (data_->play_queue.empty()) {
                    throw std::out_of_range("pop_front, playlist empty");
                }
                ensure_count(1);

                playNode &node = data_->play_queue.front();
                node.track_nod_ptr->second.erase(node.self_ptr);

                // track not present in playlist => remove ir
                if (node.track_nod_ptr->second.empty()) {
                    data_->tracks.erase(node.track_nod_ptr);
                }
                data_->play_queue.pop_front();

                shareable_ = true;
            }

            const std::pair<T const &, P const &> front() const {
                if (data_->play_queue.empty()) {
                    throw std::out_of_range("front, playlist empty");
                }
                
                playNode &node = data_->play_queue.front();
                return {node.track_nod_ptr->first, node.params};
            }

            void remove(T const &track) {
                auto map_it = data_->tracks.find(track);
                if (map_it == data_->tracks.end()) {
                    throw std::invalid_argument("remove, unknown track");
                }
                ensure_count(1);
                // after here, only destructors, so nothing should be thrown
                map_it = data_->tracks.find(track);
                
                auto &occurrences = map_it->second;
                for (auto &queue_it : occurrences) {
                    data_->play_queue.erase(queue_it);
                }
                data_->tracks.erase(map_it);

                shareable_ = true;
            }

            void clear() {
                data_ = std::make_shared<playlistData>();
            }

            size_t size() const noexcept {
                return data_->play_queue.size();
            }

            // Iterators implementation
            class play_iterator {
                // Declaring friendship, so we can hide * and -> operands.
                friend class playlist;

                public:
                    using iterator_category = std::forward_iterator_tag;
                    using value_type = playNode;
                    using difference_type = std::ptrdiff_t;
                    using pointer = p_queue_iter;
                    using reference = value_type&;

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

                    play_iterator(pointer p = nullptr): ptr{p} {}

                    reference operator*() const noexcept {
                        return *ptr;
                    }

                    pointer operator->() const noexcept {
                        return ptr;
                    }
            };

            class sorted_iterator {
                // Declaring friendship, so we can hide * and -> operands.
                friend class playlist;

                public:
                    using iterator_category = std::forward_iterator_tag;
                    using value_type = std::pair<T, std::list<p_queue_iter>>;
                    using difference_type = std::ptrdiff_t;
                    using pointer = typename track_map::const_iterator;
                    using reference = const value_type&;

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

                    sorted_iterator(pointer p = nullptr): ptr{p} {}

                    reference operator*() const noexcept {
                        return *ptr;
                    }

                    pointer operator->() const noexcept {
                        return ptr;
                    }
            };

            const std::pair<T const &, P const &> play(play_iterator const &it)
            const {
                return {it->track_nod_ptr->first, it->params};
            }

            const std::pair<T const &, size_t> pay(sorted_iterator const &it)
            const {
                return {it->first, it->second.size()};
            }

            /* Only function returning modifying refernce to the user. That's
             * why it needs to set sharable_ to false. It can throw both
             * when it is incorrect or if making a copy of internal state fails,
             * but guarantees strong exception safety, by using backup - 'copy'.
             */
            P & params(play_iterator const &it) {
                auto copy = data_;
                P * res;

                try {
                    res = &(it->params);
                    if (data_.use_count() > 2) {
                        data_ = std::make_shared<playlistData>();
                        auto & pq = copy->play_queue;

                        // We can't jsut use playlistData copy constructor, as
                        // we need to make sure we give to user reference to
                        // correct, freshly created, playNode params.
                        for (auto it2 = pq.begin(); it2 != pq.end(); ++it2) {
                            data_->push_back(
                                        it2->track_nod_ptr->first, 
                                        it2->params);
                            if (it2 == it.ptr) {
                                res = &(data_->play_queue.back().params);
                            }
                        }
                    }
                } catch (...) {
                    data_ = copy;
                    throw;
                }

                shareable_ = false;
                return *res;
            }

            // Rest of functions giving user access to the structure.
            const P & params(play_iterator const &it) const {
                return it->params;
            }

            play_iterator play_begin() const noexcept {
                return play_iterator(data_->play_queue.begin());
            }

            play_iterator play_end() const noexcept {
                return play_iterator(data_->play_queue.end());
            }

            sorted_iterator sorted_begin() const noexcept {
                return sorted_iterator(data_->tracks.begin());
            }

            sorted_iterator sorted_end() const noexcept {
                return sorted_iterator(data_->tracks.end());
            }
    };

} // namespace cxx

#endif //PLAYLIST_H