#include "trie.h"

void* trie::insert(const std::string& prefix, const void *data)
{
    trie_node* cursor = (&root), *tmp = nullptr;
    std::shared_ptr<trie_node> cur_node;

    // Iterate through the key char by char
    for (auto key: prefix)
    {
        tmp = cursor->children[key-32].get();

        // No match, we add a new node and sort the list with the new added link
        if (!tmp)
        {
//            cur_node = trie_create_node(*key);
            cur_node = std::make_shared<trie_node>();
//            cursor->children = list_push(cursor->children, cur_node);
            cursor->children[key-32] = cur_node;
            cursor->children_num++;
//            cursor->children->head = merge_sort_tnode(cursor->children->head);
        } else
        {
            // Match found, no need to sort the list, the child already exists
//            cur_node = tmp->data;
            cur_node = cursor->children[key-32];
        }
        cursor = cur_node.get();
    }

    /*
     * Clear out if already taken (e.g. we are in a leaf node), rc = 0 to not
     * change the trie size, otherwise 1 means that we added a new node,
     * effectively changing the size
     */
    if (!cursor->data)
        size++;
    cursor->data = (void *) data;
    return cursor->data;
}

trie_node* trie::find(const std::string& prefix)
{
    trie_node* retnode = &root;

    // Move to the end of the prefix first
    for (auto key: prefix)
    {
        trie_node* child = retnode->children[key-32].get();

        // No key with the full prefix in the trie
        if (!child)
            return nullptr;
        retnode = child;
    }

    return retnode;
}

void trie::recursive_apply_func(trie_node *node, void (*func)(trie_node*, void *), void* arg)
{
    if (node->data)
        func(node, arg);

    uint child_num = node->children_num;
    for (uint i = 0; i < ALPHABET_SIZE && child_num; i++)
    {
        if (node->children[i])
        {
            trie_node* child = node->children[i].get();
//            if (child->data)
//                func(child->data);
            child_num--;
            recursive_apply_func(child, func, arg);
        }
    }
}

void trie::apply_func(const std::string &prefix, void (*func)(trie_node *, void *), void* arg)
{
    if (prefix.size())
    {
        trie_node *node = find(prefix);
        if (node)
            recursive_apply_func(node, func, arg);
    }
    else
        recursive_apply_func(&root, func, arg);
}

bool trie::erase(const std::string& prefix)
{
    bool found = false;
    uint16_t index = 0;
    if (prefix.size())
        recursive_erase(root, prefix, index);
    return found;
}

bool trie::recursive_erase(trie_node& node, const std::string& key, uint16_t index)
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
