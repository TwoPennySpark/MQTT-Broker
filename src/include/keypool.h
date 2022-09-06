#ifndef KEYPOOL_H
#define KEYPOOL_H

#include <iostream>
#include <vector>
#include <set>
#include <deque>

// key pool consists of chunks
// each chunk is an interval containing 1 or more unique keys (uids)
// chunks are sorted by the key that represents the start of the interval
// each key has a corresponding value
// basically this structure is somewhat like a hashmap that provides functions for inserting
// key-value w/o explicitly specifing key, instead it searches for the first avaliable
// key in range [std::numeric_limits<KeyType>::min(), std::numeric_limits<KeyType>::max()]
// inserted keys are grouped in chunks for faster search of first unused key
template <typename KeyType, typename ValueType,
          typename = typename std::enable_if_t<std::is_integral_v<KeyType>>>
struct KeyPool
{
    struct chunk
    {
        KeyType start;
        mutable KeyType end;
        // corresponding values, values.size() == end-start+1
        mutable std::deque<ValueType> values;

        // [start, end]
        chunk(KeyType _start): start(_start){}
        chunk(KeyType _start, KeyType _end): start(_start), end(_end){}

        // chunks are sorted by 'start'
        inline bool operator< (const chunk& rhs) const
        { return (start < rhs.start) ? true : false; }

        friend std::ostream& operator<<(std::ostream& os, const chunk &c)
        {
            os << "{";
            for (KeyType i = c.start; i < c.end; i++)
                os << i << ", ";
            os << c.end << "}\n";
            return os;
        }

        chunk push_front(const ValueType& value) const
        {
            chunk temp(start-1, end);
            temp.values = std::move(values);
            temp.values.push_front(value);

            return temp;
        }

        std::optional<KeyPool::chunk> pop_front() const
        {
            if (end-start == 0)
                return std::nullopt;

            chunk temp(start+1, end);
            values.pop_front();
            temp.values = std::move(values);

            return temp;
        }

        void push_back(const ValueType& value) const
        {
            end++;
            values.push_back(value);
        }

        void pop_back() const
        {
            end--;
            values.pop_back();
        }

        // move all data from the next chunk to this one (next.start == this.end+1)
        // [this.start, this.end] + [next.start, next.end] = [this.start, next.end]
        void merge(const chunk &next) const
        {
            end = next.end;
            for (auto& value: next.values)
                values.emplace_back(std::move(value));
        }

        // split this chunk in two (start < splitPoint < end)
        // [start, end] = [start, splitPoint), (splitPoint, end]
        chunk split(KeyType splitPoint) const
        {
            chunk next(splitPoint+1, end);

            end = splitPoint-1;

            KeyType index = splitPoint - start;
            for (KeyType i = index+1; i < values.size(); i++)
                next.values.emplace_back(std::move(values[i]));
            values.erase(std::next(values.begin(), index), values.end());

            return next;
        }
    };

    void print()
    {
        for (auto& c: chunks)
            std::cout << c;
        std::cout << "=============================\n";
    }

    KeyType generate_key(const ValueType& value)
    {
        // create first chunk
        if (!chunks.size())
        {
            chunk temp(std::numeric_limits<KeyType>::min(),
                       std::numeric_limits<KeyType>::min());
            temp.values.push_back(value);
            chunks.emplace(std::move(temp));
            return std::numeric_limits<KeyType>::min();
        }

        // get first avaliable keyy
        auto it = chunks.begin();
        if (it->end == std::numeric_limits<KeyType>::max())
            throw std::runtime_error("Out of keys");
        it->push_back(value);
        KeyType key = it->end;

        // if new key connects first chunk to the second - merge them
        if (chunks.size() > 1)
        {
            auto itnext = std::next(chunks.begin(), 1);
            if (itnext->start-1 == it->end)
            {
                it->merge(*itnext);
                chunks.erase(itnext);
            }
        }
        return key;
    }

    bool register_key(KeyType key, const ValueType& value)
    {
        // find first chunk with start >= key
        auto it = chunks.lower_bound(key);
        if (it != chunks.end())
        {
            // key already taken
            if (key == it->start)
                return false;
            // key is right before start of a chunk
            else if (key == it->start-1)
            {
                if (it != chunks.begin())
                {
                    auto itCopy = it;
                    it--;
                    // key connects two chunks - merge them
                    if (it->end+1 == key)
                    {
                        // add key to existing chunk
                        it->push_back(value);

                        it->merge(*itCopy);
                        chunks.erase(itCopy);

                        return true;
                    }
                    it++;
                }

                auto chnk = it->push_front(value);
                chunks.erase(it);
                chunks.insert(chnk);

                return true;
            }
        }

        if (it != chunks.begin())
        {
            it--;
            // key already taken
            if (it->end >= key)
                return false;
            else if (it->end+1 == key)
            {
                // add key to existing chunk
                it->push_back(value);
                return true;
            }
        }

        // create new chunk
        chunk temp(key, key);
        temp.values.push_back(value);
        chunks.emplace(std::move(temp));

        return true;
    }

    bool unregister_key(KeyType key)
    {
        // points to chunk with start >= key
        auto it = chunks.lower_bound(key);
        if (it != chunks.end() && key == it->start)
        {
            if (auto temp = it->pop_front())
                chunks.insert(temp.value());
            chunks.erase(it);

            return true;
        }

        // if there is a chunk with start < key
        if (it != chunks.begin())
        {
            it--;
            if (it->end == key)
            {
                it->pop_back();
                return true;
            }
            else if (it->end > key)
            {
                // split chunk in two
                auto next = it->split(key);
                chunks.insert(next);
                return true;
            }
        }

        return false;
    }

    std::optional<std::reference_wrapper<ValueType>> find(KeyType key)
    {
        // points to chunk with start >= key
        auto it = chunks.lower_bound(key);
        if (it != chunks.end())
            if (it->start == key)
                return it->values.front();

        if (it != chunks.begin())
        {
            it--;
            if (it->end >= key)
                return it->values[key-it->start];
        }

        return std::nullopt;
    }

    std::set<chunk> chunks;
};

#endif // KEYPOOL_H
