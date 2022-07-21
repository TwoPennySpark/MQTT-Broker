#ifndef TRIE_H
#define TRIE_H

#include <memory>
#include <vector>
#include <functional>

#define ALPHABET_SIZE 96

template<typename T>
struct trie_node
{
    trie_node() {children.reserve(ALPHABET_SIZE); children_num = 0;}
    ~trie_node()
    {
        for (auto child: children)
            if (child)
                child.reset();
        data.reset();
    }
    std::vector<std::shared_ptr<trie_node>> children; // next symbols in topic name
    uint16_t children_num; // number of non-empty spots in children array
    std::shared_ptr<T> data;  // topic structure, contains subscribers info
};

template<typename T>
struct trie
{
public:
    struct trie_node<T> root;

    void insert(const std::string& prefix, std::shared_ptr<T> data)
    {
        trie_node<T>* cursor = &root;

        // Iterate through the key char by char
        for (auto key: prefix)
        {
            auto& tmp = cursor->children[key-32];

            // No match, we add a new node
            if (!tmp.get())
            {
                tmp = std::make_shared<trie_node<T>>();
                cursor->children_num++;
            }
            cursor = tmp.get();
        }

        cursor->data = data;
    }

    trie_node<T>* find(const std::string& prefix, trie_node<T>* start = nullptr)
    {
        trie_node<T>* retnode = start ? start : &root;

        // Move to the end of the prefix first
        for (auto key: prefix)
        {
            trie_node<T>* child = retnode->children[key-32].get();

            // No key with the full prefix in the trie
            if (!child)
                return nullptr;
            retnode = child;
        }

        return retnode;
    }

    // apply function 'func' to perfix node and all prefix's children
    void apply_func(const std::string &prefix, std::function<void(trie_node<T> *)> func)
    {
        if (prefix.size())
        {
            trie_node<T> *node = find(prefix);
            if (node)
                recursive_apply_func(node, func);
        }
        else
            recursive_apply_func(&root, func);
    }

    // look for occurences of 'key', starting from 'start'+'prefix' node, and apply function 'func' to them
    void apply_func_key(const std::string &prefix, trie_node<T>* start, char until, std::function<void(trie_node<T> *)> func)
    {
        if (prefix.size())
        {
            trie_node<T> *node = find(prefix, start);
            if (node)
                recursive_apply_func_key(node, until, func);
        }
        else
            recursive_apply_func_key(&root, until, func);
    }

    void erase(const std::string& prefix)
    {
        if (prefix.size())
            recursive_erase(root, prefix, 0);
    }

private:
    bool recursive_erase(trie_node<T>& node, const std::string& key, uint16_t index)
    {
        if (index == key.size())
        {
            if (node.data)
            {
                node.data = nullptr;
                // if there is no more children delete node
                if (!node.children_num)
                    return true;
            }
        }
        else
        {
            uint next_letter = key[index]-32;
            index++;
            if (node.children[next_letter])
                if (recursive_erase(*node.children[next_letter], key, index))
                {
                    node.children[next_letter] = nullptr;
                    if (!(--node.children_num))
                        return true;
                }
        }
        return false;
    }

    void recursive_apply_func(trie_node<T> *node, std::function<void(trie_node<T> *)> func)
    {
        if (node->data)
            func(node);

        uint child_num = node->children_num;
        for (uint i = 0; i < ALPHABET_SIZE && child_num; i++)
        {
            if (node->children[i])
            {
                trie_node<T>* child = node->children[i].get();
                child_num--;
                recursive_apply_func(child, func);
            }
        }
    }

    void recursive_apply_func_key(trie_node<T> *node, char key, std::function<void(trie_node<T> *)> func)
    {
        uint child_num = node->children_num;
        for (uint i = 0; i < ALPHABET_SIZE && child_num; i++)
        {
            if (node->children[i])
            {
                trie_node<T>* child = node->children[i].get();
                child_num--;
                if (i != key-32)
                    recursive_apply_func_key(child, key, func);
                else
                    func(child);
            }
        }
    }
};

#endif // TRIE_H
