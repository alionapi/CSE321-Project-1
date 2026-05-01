#ifndef CSE321_BTREE_H
#define CSE321_BTREE_H

#include "common.h"
#include <vector>
#include <algorithm>
#include <cstddef>

class BTree {
public:
    explicit BTree(int order)
        : d_(order), MAX_KEYS_(2 * order), MIN_KEYS_(order),
          root_(new Node(true)) {}

    ~BTree() { destroy(root_); }
    BTree(const BTree&)            = delete;
    BTree& operator=(const BTree&) = delete;
    bool search(int key, RID& out_rid) const {
        return search_node(root_, key, out_rid);
    }
    void insert(int key, RID rid) {
        InsertResult res = insert_into(root_, key, rid);
        if (res.split) {
            Node* new_root  = new Node(false);
            new_root->keys.push_back(res.up_key);
            new_root->rids.push_back(res.up_rid);
            new_root->children.push_back(root_);
            new_root->children.push_back(res.right);
            root_ = new_root;
        }
    }
    bool remove(int key) {
        bool ok = delete_from(root_, key);
        if (!root_->is_leaf && root_->keys.empty()) {
            Node* old = root_;
            root_     = root_->children[0];
            old->children.clear();
            delete old;
        }
        return ok;
    }
    template <class F>
    void range_query(int lo, int hi, F visit) const {
        range_walk(root_, lo, hi, visit);
    }
    long long split_count()  const { return splits_; }
    long long node_count()   const { return count_nodes(root_); }
    long long key_count()    const { return count_keys(root_); }
    int       height()       const { return height_of(root_); }

    double    utilization()  const {
        long long n = node_count();
        if (n == 0) return 0.0;
        return double(key_count()) / double(n * MAX_KEYS_);
    }
private:
    struct Node {
        bool is_leaf;
        std::vector<int>   keys;
        std::vector<RID>   rids;
        std::vector<Node*> children;   // empty if is_leaf
        explicit Node(bool leaf) : is_leaf(leaf) {}
    };
    int   d_;
    int   MAX_KEYS_;
    int   MIN_KEYS_;
    Node* root_;
    long long splits_ = 0;
    static void destroy(Node* n) {
        if (!n) return;
        if (!n->is_leaf) for (Node* c : n->children) destroy(c);
        delete n;
    }
    static long long count_nodes(Node* n) {
        if (!n) return 0;
        long long s = 1;
        if (!n->is_leaf) for (Node* c : n->children) s += count_nodes(c);
        return s;
    }
    static long long count_keys(Node* n) {
        if (!n) return 0;
        long long s = (long long)n->keys.size();
        if (!n->is_leaf) for (Node* c : n->children) s += count_keys(c);
        return s;
    }
    static int height_of(Node* n) {
        if (!n) return 0;
        if (n->is_leaf) return 1;
        return 1 + height_of(n->children[0]);
    }
    bool search_node(Node* n, int key, RID& out) const {
        size_t i = 0;
        while (i < n->keys.size() && key > n->keys[i]) ++i;
        if (i < n->keys.size() && key == n->keys[i]) {
            out = n->rids[i];
            return true;
        }
        if (n->is_leaf) return false;
        return search_node(n->children[i], key, out);
    }
    struct InsertResult {
        bool  split   = false;
        int   up_key  = 0;
        RID   up_rid  = 0;
        Node* right   = nullptr;
    };
    InsertResult insert_into(Node* n, int key, RID rid) {
        size_t i = 0;
        while (i < n->keys.size() && key > n->keys[i]) ++i;
        if (i < n->keys.size() && key == n->keys[i]) {
            n->rids[i] = rid;
            return {};
        }
        if (n->is_leaf) {
            n->keys.insert(n->keys.begin() + i, key);
            n->rids.insert(n->rids.begin() + i, rid);
        } else {
            InsertResult child = insert_into(n->children[i], key, rid);
            if (!child.split) return {};
            n->keys.insert(n->keys.begin() + i, child.up_key);
            n->rids.insert(n->rids.begin() + i, child.up_rid);
            n->children.insert(n->children.begin() + i + 1, child.right);
        }
        if ((int)n->keys.size() > MAX_KEYS_) {
            return split_node(n);
        }
        return {};
    }
    InsertResult split_node(Node* n) {
        ++splits_;
        int mid = (int)n->keys.size() / 2;
        Node* right = new Node(n->is_leaf);
        right->keys.assign(n->keys.begin() + mid + 1, n->keys.end());
        right->rids.assign(n->rids.begin() + mid + 1, n->rids.end());
        if (!n->is_leaf) {
            right->children.assign(n->children.begin() + mid + 1, n->children.end());
            n->children.resize(mid + 1);
        }
        InsertResult r;
        r.split  = true;
        r.up_key = n->keys[mid];
        r.up_rid = n->rids[mid];
        r.right  = right;
        n->keys.resize(mid);
        n->rids.resize(mid);
        return r;
    }
    bool delete_from(Node* n, int key) {
        size_t i = 0;
        while (i < n->keys.size() && key > n->keys[i]) ++i;
        if (i < n->keys.size() && n->keys[i] == key) {
            if (n->is_leaf) {
                n->keys.erase(n->keys.begin() + i);
                n->rids.erase(n->rids.begin() + i);
                return true;
            }
            Node* succ_node = n->children[i + 1];
            while (!succ_node->is_leaf) succ_node = succ_node->children.front();
            int succ_key = succ_node->keys.front();
            RID succ_rid = succ_node->rids.front();
            n->keys[i] = succ_key;
            n->rids[i] = succ_rid;
            bool ok = delete_from(n->children[i + 1], succ_key);
            fix_underflow_if_needed(n, i + 1);
            return ok;
        }
        if (n->is_leaf) return false;  // key not present
        bool ok = delete_from(n->children[i], key);
        fix_underflow_if_needed(n, i);
        return ok;
    }
    void fix_underflow_if_needed(Node* parent, size_t ci) {
        Node* c = parent->children[ci];
        if ((int)c->keys.size() >= MIN_KEYS_) return;
        Node* left  = ci > 0 ? parent->children[ci - 1] : nullptr;
        Node* right = ci + 1 < parent->children.size() ? parent->children[ci + 1] : nullptr;
        if (left && (int)left->keys.size() > MIN_KEYS_) {
            c->keys.insert(c->keys.begin(), parent->keys[ci - 1]);
            c->rids.insert(c->rids.begin(), parent->rids[ci - 1]);
            parent->keys[ci - 1] = left->keys.back();
            parent->rids[ci - 1] = left->rids.back();
            left->keys.pop_back();
            left->rids.pop_back();
            if (!left->is_leaf) {
                c->children.insert(c->children.begin(), left->children.back());
                left->children.pop_back();
            }
            return;
        }
        if (right && (int)right->keys.size() > MIN_KEYS_) {
            c->keys.push_back(parent->keys[ci]);
            c->rids.push_back(parent->rids[ci]);
            parent->keys[ci] = right->keys.front();
            parent->rids[ci] = right->rids.front();
            right->keys.erase(right->keys.begin());
            right->rids.erase(right->rids.begin());
            if (!right->is_leaf) {
                c->children.push_back(right->children.front());
                right->children.erase(right->children.begin());
            }
            return;
        }
        if (left) {
            merge_children(parent, ci - 1);
        } else {
            merge_children(parent, ci);
        }
    }
    void merge_children(Node* parent, size_t i) {
        Node* L = parent->children[i];
        Node* R = parent->children[i + 1];

        L->keys.push_back(parent->keys[i]);
        L->rids.push_back(parent->rids[i]);
        L->keys.insert(L->keys.end(), R->keys.begin(), R->keys.end());
        L->rids.insert(L->rids.end(), R->rids.begin(), R->rids.end());
        if (!L->is_leaf) {
            L->children.insert(L->children.end(), R->children.begin(), R->children.end());
        }
        parent->keys.erase(parent->keys.begin() + i);
        parent->rids.erase(parent->rids.begin() + i);
        parent->children.erase(parent->children.begin() + i + 1);

        R->children.clear();
        delete R;
    }
    template <class F>
    void range_walk(Node* n, int lo, int hi, F& visit) const {
        if (!n) return;
        size_t i = 0;
        while (i < n->keys.size() && n->keys[i] < lo) ++i;
        for (; i < n->keys.size() && n->keys[i] <= hi; ++i) {
            if (!n->is_leaf) range_walk(n->children[i], lo, hi, visit);
            visit(n->keys[i], n->rids[i]);
        }
        if (!n->is_leaf && i < n->children.size()) {
            // The last child (at position i) may still contain keys in [lo,hi].
            range_walk(n->children[i], lo, hi, visit);
        }
    }
};
#endif // CSE321_BTREE_H
