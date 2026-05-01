#ifndef CSE321_BPLUS_TREE_H
#define CSE321_BPLUS_TREE_H

#include "common.h"
#include <vector>
#include <algorithm>

class BPlusTree {
public:
    explicit BPlusTree(int order)
        : d_(order), MAX_KEYS_(2 * order), MIN_KEYS_(order),
          root_(new Node(true)) {
        first_leaf_ = root_;
    }
    ~BPlusTree() { destroy(root_); }
    BPlusTree(const BPlusTree&)            = delete;
    BPlusTree& operator=(const BPlusTree&) = delete;
    bool search(int key, RID& out) const {
        Node* n = root_;
        while (!n->is_leaf) {
            size_t i = 0;
            while (i < n->keys.size() && key >= n->keys[i]) ++i;
            n = n->children[i];
        }
        for (size_t i = 0; i < n->keys.size(); ++i) {
            if (n->keys[i] == key) { out = n->rids[i]; return true; }
        }
        return false;
    }
    void insert(int key, RID rid) {
        InsertResult res = insert_into(root_, key, rid);
        if (res.split) {
            Node* nr = new Node(false);
            nr->keys.push_back(res.up_key);
            nr->children.push_back(root_);
            nr->children.push_back(res.right);
            root_ = nr;
        }
    }

    bool remove(int key) {
        bool ok = delete_from(root_, key);
        if (!root_->is_leaf && root_->keys.empty()) {
            Node* old = root_;
            root_ = root_->children[0];
            old->children.clear();
            delete old;
        }
        return ok;
    }
    template <class F>
    void range_query(int lo, int hi, F visit) const {
        Node* n = root_;
        while (!n->is_leaf) {
            size_t i = 0;
            while (i < n->keys.size() && lo >= n->keys[i]) ++i;
            n = n->children[i];
        }
        while (n) {
            for (size_t i = 0; i < n->keys.size(); ++i) {
                if (n->keys[i] < lo)  continue;
                if (n->keys[i] > hi)  return;
                visit(n->keys[i], n->rids[i]);
            }
            n = n->next;
        }
    }
    long long split_count() const { return splits_; }
    long long node_count()  const { return count_nodes(root_); }
    long long leaf_count()  const {
        long long c = 0;
        for (Node* n = first_leaf_; n; n = n->next) ++c;
        return c;
    }
    int       height()      const { return height_of(root_); }
    double utilization() const {
        long long used = 0, cap = 0;
        utilization_walk(root_, used, cap);
        return cap == 0 ? 0.0 : double(used) / double(cap);
    }
private:
    struct Node {
        bool is_leaf;
        std::vector<int>   keys;
        std::vector<RID>   rids;     // only used in leaves
        std::vector<Node*> children; // only used in internal nodes
        Node*              next = nullptr;  // only used in leaves (linked list)
        explicit Node(bool leaf) : is_leaf(leaf) {}
    };
    int   d_, MAX_KEYS_, MIN_KEYS_;
    Node* root_;
    Node* first_leaf_;
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
    static int height_of(Node* n) {
        if (!n) return 0;
        if (n->is_leaf) return 1;
        return 1 + height_of(n->children[0]);
    }
    void utilization_walk(Node* n, long long& used, long long& cap) const {
        cap += MAX_KEYS_;
        used += (long long)n->keys.size();
        if (!n->is_leaf) for (Node* c : n->children) utilization_walk(c, used, cap);
    }
    struct InsertResult {
        bool  split  = false;
        int   up_key = 0;
        Node* right  = nullptr;
    };
    InsertResult insert_into(Node* n, int key, RID rid) {
        if (n->is_leaf) {
            size_t i = 0;
            while (i < n->keys.size() && key > n->keys[i]) ++i;
            if (i < n->keys.size() && n->keys[i] == key) {
                n->rids[i] = rid;     // overwrite (duplicate)
                return {};
            }
            n->keys.insert(n->keys.begin() + i, key);
            n->rids.insert(n->rids.begin() + i, rid);
            if ((int)n->keys.size() <= MAX_KEYS_) return {};
            return split_leaf(n);
        }
        size_t i = 0;
        while (i < n->keys.size() && key >= n->keys[i]) ++i;
        InsertResult c = insert_into(n->children[i], key, rid);
        if (!c.split) return {};
        n->keys.insert(n->keys.begin() + i, c.up_key);
        n->children.insert(n->children.begin() + i + 1, c.right);
        if ((int)n->keys.size() <= MAX_KEYS_) return {};
        return split_internal(n);
    }
    InsertResult split_leaf(Node* n) {
        ++splits_;
        int mid = (int)n->keys.size() / 2;
        Node* right = new Node(true);
        right->keys.assign(n->keys.begin() + mid, n->keys.end());
        right->rids.assign(n->rids.begin() + mid, n->rids.end());
        n->keys.resize(mid);
        n->rids.resize(mid);
        right->next = n->next;
        n->next     = right;
        InsertResult r;
        r.split  = true;
        r.up_key = right->keys.front();   // copy up
        r.right  = right;
        return r;
    }
    InsertResult split_internal(Node* n) {
        ++splits_;
        int mid = (int)n->keys.size() / 2;
        int promoted = n->keys[mid];
        Node* right = new Node(false);
        right->keys.assign(n->keys.begin() + mid + 1, n->keys.end());
        right->children.assign(n->children.begin() + mid + 1, n->children.end());
        n->keys.resize(mid);
        n->children.resize(mid + 1);
        InsertResult r;
        r.split  = true;
        r.up_key = promoted;
        r.right  = right;
        return r;
    }
    bool delete_from(Node* n, int key) {
        if (n->is_leaf) {
            for (size_t i = 0; i < n->keys.size(); ++i) {
                if (n->keys[i] == key) {
                    n->keys.erase(n->keys.begin() + i);
                    n->rids.erase(n->rids.begin() + i);
                    return true;
                }
            }
            return false;
        }
        size_t i = 0;
        while (i < n->keys.size() && key >= n->keys[i]) ++i;
        bool ok = delete_from(n->children[i], key);
        fix_underflow(n, i);
        return ok;
    }
    void fix_underflow(Node* parent, size_t ci) {
        Node* c = parent->children[ci];
        // The root is allowed to fall below MIN_KEYS_.
        int min = MIN_KEYS_;
        if ((int)c->keys.size() >= min) return;
        Node* L = ci > 0 ? parent->children[ci - 1] : nullptr;
        Node* R = ci + 1 < parent->children.size() ? parent->children[ci + 1] : nullptr;
        if (c->is_leaf) {
            // ----- leaf borrow / merge -----
            if (L && (int)L->keys.size() > min) {
                // Borrow last entry of L.
                c->keys.insert(c->keys.begin(), L->keys.back());
                c->rids.insert(c->rids.begin(), L->rids.back());
                L->keys.pop_back(); L->rids.pop_back();
                parent->keys[ci - 1] = c->keys.front();   // update separator
                return;
            }
            if (R && (int)R->keys.size() > min) {
                c->keys.push_back(R->keys.front());
                c->rids.push_back(R->rids.front());
                R->keys.erase(R->keys.begin()); R->rids.erase(R->rids.begin());
                parent->keys[ci] = R->keys.front();
                return;
            }
            if (L) merge_leaves(parent, ci - 1);
            else   merge_leaves(parent, ci);
            return;
        }
        if (L && (int)L->keys.size() > min) {
            // Rotate via parent separator.
            c->keys.insert(c->keys.begin(), parent->keys[ci - 1]);
            c->children.insert(c->children.begin(), L->children.back());
            parent->keys[ci - 1] = L->keys.back();
            L->keys.pop_back(); L->children.pop_back();
            return;
        }
        if (R && (int)R->keys.size() > min) {
            c->keys.push_back(parent->keys[ci]);
            c->children.push_back(R->children.front());
            parent->keys[ci] = R->keys.front();
            R->keys.erase(R->keys.begin()); R->children.erase(R->children.begin());
            return;
        }
        if (L) merge_internals(parent, ci - 1);
        else   merge_internals(parent, ci);
    }

    void merge_leaves(Node* parent, size_t i) {
        Node* L = parent->children[i];
        Node* R = parent->children[i + 1];
        L->keys.insert(L->keys.end(), R->keys.begin(), R->keys.end());
        L->rids.insert(L->rids.end(), R->rids.begin(), R->rids.end());
        L->next = R->next;          // keep leaf chain intact
        parent->keys.erase(parent->keys.begin() + i);
        parent->children.erase(parent->children.begin() + i + 1);
        delete R;
    }
    void merge_internals(Node* parent, size_t i) {
        Node* L = parent->children[i];
        Node* R = parent->children[i + 1];
        L->keys.push_back(parent->keys[i]);
        L->keys.insert(L->keys.end(), R->keys.begin(), R->keys.end());
        L->children.insert(L->children.end(), R->children.begin(), R->children.end());
        parent->keys.erase(parent->keys.begin() + i);
        parent->children.erase(parent->children.begin() + i + 1);
        R->children.clear();
        delete R;
    }
};

#endif // CSE321_BPLUS_TREE_H
