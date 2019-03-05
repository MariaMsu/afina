#include <utility>
#include <iostream>

#include "SimpleLRU.h"

namespace Afina {
    namespace Backend {

        // See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Put(const std::string &key, const std::string &value) {
            //there is object with the key
            if (_lru_index.find(std::reference_wrapper<const std::string>(key)) != _lru_index.end())
                return Set(key, value);

            return insert_new_node(key, value);
        }

        // See MapBasedGlobalLockImpl.h
        bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
            //there is object with the key
            if (_lru_index.find(std::reference_wrapper<const std::string>(key)) != _lru_index.end())
                return false;

            return insert_new_node(key, value);
        }


        // See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Set(const std::string &key, const std::string &value) {
            //there is no object with the key
            if (_lru_index.find(std::reference_wrapper<const std::string>(key)) == _lru_index.end())
                return false;

            //memory overruns
            if (key.size()
                + value.size()
                + _current_size
                - _lru_index.at(key).get().value.size()
                > _max_size)
                return false;
//todo difference std::wrapper?
            _lru_index.at(std::reference_wrapper<const std::string>(key)) = std::reference_wrapper<lru_node>(
                    *create_new_node(key, value));

            move_to_tail(key);
            return true;
        }

        // See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Delete(const std::string &key) {
            auto del_node = _lru_index.find(std::reference_wrapper<const std::string>(key));

            //there is no object with the key
            if (del_node == _lru_index.end())
                return false;

            swap((*del_node).second.get().prev->next, (*del_node).second.get().next);
            delete &(*del_node).second.get();
            _lru_index.erase(del_node);
            return true;
        }

        // See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Get(const std::string &key, std::string &value) {
            //there is no object with the key
            if (_lru_index.find(std::reference_wrapper<const std::string>(key)) == _lru_index.end()) {
                value = "";
                return false;
            }

            value = _lru_index.at(std::reference_wrapper<const std::string>(key)).get().value;
            move_to_tail(key);
            return true;
        }

        bool SimpleLRU::delete_oldest_node() {
            if (_lru_head->next->next = nullptr)
                return false;
            _current_size -= _lru_head->key.size() + _lru_head->value.size();
            std::string key = _lru_head->key;
            swap(_lru_head->next, _lru_head->next->next);
            _lru_index.erase(key);
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
            auto mov_node = _lru_index.find(std::reference_wrapper<const std::string>(key));

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
            _lru_tail = current_node;
        }

        bool SimpleLRU::insert_new_node(const std::string &key, const std::string &value) {
            if (key.size() + value.size() > _max_size)
                return false;

            while (key.size() + value.size() + _current_size > _max_size)
                delete_oldest_node();

            lru_node *new_node = create_new_node(key, value);
            _current_size += key.size() + value.size();
            return _lru_index.insert({std::reference_wrapper<const std::string>(new_node->key),
                                      std::reference_wrapper<lru_node>(*new_node)}).second;
        }
    } // namespace Backend
} // namespace Afina
