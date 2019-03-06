#include <utility>

#include "SimpleLRU.h"

namespace Afina {
    namespace Backend {

        // See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Put(const std::string &key, const std::string &value) {
            //there is object with the key
            if (_lru_index.find(key) != _lru_index.end())
                return Set(key, value);

            return insert_new_node(key, value);
        }

        // See MapBasedGlobalLockImpl.h
        bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
            //there is object with the key
            if (_lru_index.find(key) != _lru_index.end())
                return false;

            return insert_new_node(key, value);
        }


        // See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Set(const std::string &key, const std::string &value) {
            //there is no object with the key
            if (_lru_index.find(key) == _lru_index.end())
                return false;

            //memory overruns
            if (key.size()
                + value.size()
                + _current_size
                - _lru_index.at(key).get().value.size()
                > _max_size)
                return false;
            _lru_index.at(key) = *create_new_node(key, value);

            move_to_tail(key);
            return true;
        }

        // See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Delete(const std::string &key) {
            auto del_node = _lru_index.find(key);

            //there is no object with the key
            if (del_node == _lru_index.end())
                return false;

            _lru_index.erase(key);

            swap((*del_node).second.get().prev->next, (*del_node).second.get().next);
            (*del_node).second.get().next = nullptr;
            return true;
        }

        // See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Get(const std::string &key, std::string &value) {
            //there is no object with the key
            if (_lru_index.find(key) == _lru_index.end()) {
                value = "";
                return false;
            }

            value = _lru_index.at(key).get().value;
            move_to_tail(key);
            return true;
        }

        bool SimpleLRU::delete_oldest_node() {
            lru_node *old_node = _lru_head->next.get();
            if (old_node == nullptr)
                return false;
            _current_size -= _lru_head->next->key.size() + _lru_head->next->value.size();
            _lru_index.erase(old_node->key);
            old_node->next->prev = _lru_head.get();
            swap(_lru_head->next, old_node->next);
            old_node->next = nullptr;
            return true;
        }

        SimpleLRU::lru_node *SimpleLRU::create_new_node(std::string key, std::string value) {
            auto *new_node = new lru_node;
            new_node->key = std::move(key);
            new_node->value = std::move(value);
            new_node->prev = _lru_tail->prev;
            new_node->next = std::unique_ptr<lru_node>(new_node);

            swap(_lru_tail->prev->next, new_node->next);

            _lru_tail->prev = new_node;

            return new_node;
        }

        void SimpleLRU::move_to_tail(const std::string &key) {
            auto mov_node = _lru_index.find(key);

            //there is no object with the key
            if (mov_node == _lru_index.end()) {
                return;
            }

            lru_node *current_node = &(*mov_node).second.get();

            //current node is already last
            if (_lru_tail == current_node)
                return;

            current_node->next->prev = current_node->prev;
            swap(current_node->prev->next, current_node->next);
            current_node->prev = _lru_tail->prev;
            swap(current_node->next, _lru_tail->prev->next);
            _lru_tail->prev = current_node;
        }

        bool SimpleLRU::insert_new_node(const std::string &key, const std::string &value) {
            if (key.size() + value.size() > _max_size)
                return false;

            while (key.size() + value.size() + _current_size > _max_size)
                delete_oldest_node();

            lru_node *new_node = create_new_node(key, value);
            _current_size += key.size() + value.size();
            return _lru_index.insert({new_node->key, *new_node}).second;
        }
    } // namespace Backend
} // namespace Afina
