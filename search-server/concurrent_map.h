#pragma once
#include <map>
#include <mutex>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count) {
    }

    Access operator[](const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return { std::lock_guard<std::mutex>(bucket.mutex), bucket.map[key] };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mutex, map] : buckets_) {
            std::lock_guard g(mutex);
            result.insert(map.begin(), map.end());
        }
        return result;
    }
    
    void Erase(const Key& key) {
        auto& bucket = buckets_[key % buckets_.size()];
        std::lock_guard<std::mutex> g(bucket.mutex);
        bucket.map.erase(key);
    }

private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };
    std::vector<Bucket> buckets_;
};