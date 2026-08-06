#pragma once

#include <unordered_map>

namespace iris {

template <typename Key, typename Value> class bidirectional_map {
    std::unordered_map <Key, Value> m_forward_map;
    std::unordered_map <Value, Key> m_reverse_map;

public:
    void insert(const Key& key, const Value& value) {
        m_forward_map[key] = value;
        m_reverse_map[value] = key;
    }

    std::unordered_map <Key, Value>& forward_map() {
        return m_forward_map;
    }

    std::unordered_map <Value, Key>& reverse_map() {
        return m_reverse_map;
    }

    bool erase_by_key(const Key& key) {
        auto it = m_forward_map.find(key);
        if (it != m_forward_map.end()) {
            Value value = it->second;
            m_forward_map.erase(it);
            m_reverse_map.erase(value);
            return true;
        }
        return false;
    }

    bool erase_by_value(const Value& value) {
        auto it = m_reverse_map.find(value);
        if (it != m_reverse_map.end()) {
            Key key = it->second;
            m_reverse_map.erase(it);
            m_forward_map.erase(key);
            return true;
        }
        return false;
    }

    void clear() {
        m_forward_map.clear();
        m_reverse_map.clear();
    }

    Value* get_value(const Key& key) {
        auto it = m_forward_map.find(key);
        if (it != m_forward_map.end()) {
            return &it->second;
        }
        return nullptr;
    }

    Key* get_key(const Value& value) {
        auto it = m_reverse_map.find(value);
        if (it != m_reverse_map.end()) {
            return &it->second;
        }
        return nullptr;
    }
};

}
