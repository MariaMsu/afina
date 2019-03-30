#include <utility>

#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    auto iterator = _lru_index.find(key);
    //there is object with the key
    if (iterator != _lru_index.end())
        return change_value(iterator, value);

    return insert_new_node(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    auto iterator = _lru_index.find(key);
    //there is object with the key
    if (iterator != _lru_index.end())
        return false;

    return insert_new_node(key, value);
}


// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto iterator = _lru_index.find(key);

    //there is no object with the key
    if (iterator == _lru_index.end())
        return false;

    return change_value(iterator, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto del_node = _lru_index.find(key);

    //there is no object with the key
    if (del_node == _lru_index.end())
        return false;

    _lru_index.erase(key);

    (*del_node).second.get().next->prev = (*del_node).second.get().prev;
    swap((*del_node).second.get().prev->next, (*del_node).second.get().next);
    (*del_node).second.get().next = nullptr;
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto iterator = _lru_index.find(key);

    //there is no object with the key
    if (iterator == _lru_index.end()) {
        return false;
    }

    value = (*iterator).second.get().value;
    move_to_tail(iterator);
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

SimpleLRU::lru_node &SimpleLRU::create_new_node(std::string key, std::string value) {
    lru_node *new_node = new lru_node{std::move(key),
                                      std::move(value),
                                      _lru_tail->prev,
                                      nullptr};
    new_node->next = std::unique_ptr<lru_node>(new_node);

    swap(_lru_tail->prev->next, new_node->next);

    _lru_tail->prev = new_node;

    return *new_node;
}

void SimpleLRU::move_to_tail(
        std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>>::iterator iterator) {
    lru_node &current_node = (*iterator).second.get();

    //current node is already last
    if (_lru_tail->key == current_node.key)
        return;

    current_node.next->prev = current_node.prev;
    swap(current_node.prev->next, current_node.next);
    current_node.prev = _lru_tail->prev;
    swap(current_node.next, _lru_tail->prev->next);
    _lru_tail->prev = &current_node;
}

//std::map<std::reference_wrapper<std::string>, std::reference_wrapper<lru_node>>::iterator iterator,
bool SimpleLRU::insert_new_node(const std::string &key, const std::string &value) {

    if (key.size() + value.size() > _max_size)
        return false;

    while (key.size() + value.size() + _current_size > _max_size)
        delete_oldest_node();

    lru_node &new_node = create_new_node(key, value);
    _current_size += key.size() + value.size();

    return _lru_index.insert(std::make_pair(std::reference_wrapper<const std::string>(new_node.key),
                                            std::reference_wrapper<lru_node>(new_node))).second;
}

bool SimpleLRU::change_value(
        std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>>::iterator iterator,
        const std::string &value) {

    lru_node &current_node = (*iterator).second.get();

    //memory overruns
    if (current_node.key.size() - current_node.value.size() + value.size() > _max_size)
        return false;

    while (current_node.key.size() - current_node.value.size() + value.size() + _current_size > _max_size)
        delete_oldest_node();

    current_node.value = value;

    move_to_tail(iterator);
    return true;
}

} // namespace Backend
} // namespace Afina
