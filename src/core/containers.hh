#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

using String = std::string;

template <typename T>
using Vector = std::vector<T>;

template <typename Key, typename T>
using HashMap = std::map<Key, T>;

template <typename T>
using HashSet = std::set<T>;
