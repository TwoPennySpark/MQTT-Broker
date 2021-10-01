#ifndef TRIE_H
#define TRIE_H

#include <memory>
#include <vector>

#define ALPHABET_SIZE 96

struct trie_node
{
    trie_node() {children.reserve(ALPHABET_SIZE); children_num = 0;}
    std::vector<std::shared_ptr<trie_node>> children; // next symbols in topic name
    uint16_t children_num; // number of non-empty spots in children array
    void *data;  // topic structure, contains subscribers info
};

struct trie
{
public:
    trie(): size(0){}
    struct trie_node root;
    size_t size;

    void *insert(const std::string &prefix, const void* data);
    trie_node *find(const std::string &prefix);
    bool erase(const std::string &prefix);

    typedef void (*mapfunc)(void *);
    void apply_func(const std::string &prefix, void (*func)(trie_node *, void *), void *arg);

private:
    bool recursive_erase(trie_node &node, const std::string &prefix, uint16_t index);
    void recursive_apply_func(trie_node *node, void (*func)(trie_node *, void *), void* arg);
};

#endif // TRIE_H
