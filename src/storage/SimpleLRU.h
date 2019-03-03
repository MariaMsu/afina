#include <utility>

#include <utility>

#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <afina/Storage.h>

namespace Afina {
    namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
        class SimpleLRU : public Afina::Storage {
        public:
            SimpleLRU(size_t max_size = 1024) : _max_size(max_size) {}

            ~SimpleLRU() {
                _lru_index.clear();
                _lru_head.reset(); // TODO: Here is stack overflow
            }

            // Implements Afina::Storage interface
            bool Put(const std::string &key, const std::string &value) override;

            // Implements Afina::Storage interface
            bool PutIfAbsent(const std::string &key, const std::string &value) override;

            // Implements Afina::Storage interface
            bool Set(const std::string &key, const std::string &value) override;

            // Implements Afina::Storage interface
            bool Delete(const std::string &key) override;

            // Implements Afina::Storage interface
            bool Get(const std::string &key, std::string &value) override;

        private:
            // LRU cache node
            using lru_node = struct lru_node {
                std::string key;
                std::string value;
                lru_node *prev;
                std::unique_ptr<lru_node> next;

//                lru_node(std::string key, std::string value, lru_node *tail) :
//                        key(std::move(key)), value(std::move(value)),
//                        prev(nullptr),
//                        next(nullptr) {
//                    std::unique_ptr<lru_node>(this).swap(tail->next);
//                    tail = this;
//                }
//
//                ~lru_node() {
//                    prev->next.swap(next);
//                }
            };

            // Maximum number of bytes could be stored in this cache.
            // i.e all (keys+values) must be less the _max_size
            std::size_t _max_size;

            // Current total size of stored bytes
            std::size_t _current_size;

            // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
            // element that wasn't used for longest time.
            //
            // List owns all nodes
            // null if cash is empty
            std::unique_ptr<lru_node> _lru_head = nullptr;

            // newest element
            // null if cash is empty
            lru_node *_lru_tail = nullptr;

            // Index of nodes from list above, allows fast random access to elements by lru_node#key
            std::map<std::reference_wrapper<const std::string>,
                    std::reference_wrapper<lru_node>> _lru_index;

            bool delete_oldest_node();

            lru_node *create_new_node(std::string key, std::string value);

            void move_to_tail(const std::string& key);
        };

    } // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
