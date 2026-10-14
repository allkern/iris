/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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
