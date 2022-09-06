#ifndef TRIE_H
#define TRIE_H

#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>

#define ALPHABET_SIZE 96

template<typename T>
struct trie_node
{
    trie_node() {children.reserve(ALPHABET_SIZE/6);}
    ~trie_node() {}
    std::unordered_map<char, std::unique_ptr<trie_node>> children; // list of children (child is a next symbol in topic name)
    std::shared_ptr<T> data;  // data associated with node (topic structure, contains subscribers info)
};

template<typename T>
struct trie
{
public:
    struct trie_node<T> root;

    void insert(const std::string& prefix, const std::shared_ptr<T>& data)
    {
        trie_node<T>* cursor = &root;

        // Iterate through the key char by char
        for (auto key: prefix)
        {
            auto it = cursor->children.find(key);

            // No match, we add a new node
            if (it == cursor->children.end())
                it = cursor->children.emplace(key, std::make_unique<trie_node<T>>()).first;
            cursor = it->second.get();
        }

        cursor->data = data;
    }

    // look for 'prefix' node starting from 'start' node or root
    trie_node<T>* find(const std::string& prefix, trie_node<T>* start = nullptr)
    {
        trie_node<T>* retnode = start ? start : &root;

        // Move to the end of the prefix first
        for (auto key: prefix)
        {
            auto it = retnode->children.find(key);
            // No key with the full prefix in the trie
            if (it == retnode->children.end())
                return nullptr;

            retnode = it->second.get();
        }

        return retnode;
    }

    // apply function 'func' to perfix node and all prefix's children
    void apply_func(const std::string &prefix, trie_node<T>* start, std::function<void(trie_node<T> *)> func)
    {
        if (!start)
            start = &root;
        trie_node<T> *node = find(prefix, start);
        if (node)
            recursive_apply_func(node, func);
    }

    // look for occurences of 'key', starting from 'start'+'prefix' node, and apply function 'func' to them
    void apply_func_key(const std::string &prefix, trie_node<T>* start, char key, std::function<void(trie_node<T> *)> func)
    {
        if (!start)
            start = &root;
        trie_node<T> *node = find(prefix, start);
        if (node)
            recursive_apply_func_key(node, key, func);
    }

    void find_all_data_until(const std::string &prefix, trie_node<T>* start, char until, std::function<void(trie_node<T> *)> func)
    {
        if (!start)
            start = &root;
        trie_node<T> *node = find(prefix, start);
        if (node)
            recursive_apply_data_until(node, until, func);
    }

    void erase(const std::string& topicName)
    {
        if (topicName.size())
            recursive_erase(root, topicName, 0);
    }

private:
    bool recursive_erase(trie_node<T>& node, const std::string& key, uint16_t index)
    {
        if (index == key.size())
        {
            if (node.data)
            {
                node.data.reset();
                // if there is no more children delete node
                if (!node.children.size())
                    return true;
            }
        }
        else
        {
            char next_letter = key[index++];
            auto it = node.children.find(next_letter);
            if (it != node.children.end())
                if (recursive_erase(*node.children[next_letter], key, index))
                {
                    node.children.erase(next_letter);
                    if (!node.children.size() && !node.data)
                        return true;
                }
        }
        return false;
    }

    void recursive_apply_func(trie_node<T> *node, std::function<void(trie_node<T> *)> func)
    {
        if (node->data)
            func(node);

        for (auto& child: node->children)
            recursive_apply_func(child.second.get(), func);
    }

    void recursive_apply_func_key(trie_node<T> *node, char key, std::function<void(trie_node<T> *)> func)
    {
        for (auto& child: node->children)
        {
            if (child.first != key)
                recursive_apply_func_key(child.second.get(), key, func);
            else
                func(child.second.get());
        }
    }

    void recursive_apply_data_until(trie_node<T> *node, char until, std::function<void(trie_node<T> *)> func)
    {
        if (node->data)
            func(node);

        for (auto& child: node->children)
            if (child.first != until)
                recursive_apply_data_until(child.second.get(), until, func);
    }
};

#endif // TRIE_H
