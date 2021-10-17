#ifndef TRIE_H
#define TRIE_H

#include <memory>
#include <vector>

#define ALPHABET_SIZE 96

template<typename T>
struct trie_node
{
    trie_node() {children.reserve(ALPHABET_SIZE); children_num = 0;}
    std::vector<std::shared_ptr<trie_node>> children; // next symbols in topic name
    uint16_t children_num; // number of non-empty spots in children array
    std::shared_ptr<T> data;  // topic structure, contains subscribers info
};

template<typename T>
struct trie
{
public:
    struct trie_node<T> root;

    void insert(const std::string& prefix, T* data)
    {
        trie_node<T>* cursor = (&root), *cur_node = nullptr,
                    *tmp = nullptr;
//        std::shared_ptr<trie_node<T>> cur_node;

        // Iterate through the key char by char
        for (auto key: prefix)
        {
            tmp = cursor->children[key-32].get();

            // No match, we add a new node
            if (!tmp)
            {
//                cur_node = std::make_shared<trie_node<T>>();
                cur_node = new trie_node<T>;
                cursor->children[key-32] = std::shared_ptr<trie_node<T>>(cur_node);
                cursor->children_num++;
            } else
            {
                // Match found, the child already exists
                cur_node = cursor->children[key-32].get();
            }
            cursor = cur_node;
        }

        cursor->data = std::shared_ptr<T>(data);
    }

    trie_node<T>* find(const std::string& prefix)
    {
        trie_node<T>* retnode = &root;

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

    // apply function 'func' with argument 'arg' to perfix node and all prefix's children
    void apply_func(const std::string &prefix, void (*func)(trie_node<T> *, void *), void* arg)
    {
        if (prefix.size())
        {
            trie_node<T> *node = find(prefix);
            if (node)
                recursive_apply_func(node, func, arg);
        }
        else
            recursive_apply_func(&root, func, arg);
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

    void recursive_apply_func(trie_node<T> *node, void (*func)(trie_node<T> *, void *), void* arg)
    {
        if (node->data)
            func(node, arg);

        uint child_num = node->children_num;
        for (uint i = 0; i < ALPHABET_SIZE && child_num; i++)
        {
            if (node->children[i])
            {
                trie_node<T>* child = node->children[i].get();
                child_num--;
                recursive_apply_func(child, func, arg);
            }
        }
    }
};

#endif // TRIE_H
