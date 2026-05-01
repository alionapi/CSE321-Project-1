#ifndef CSE321_BSTAR_TREE_H
#define CSE321_BSTAR_TREE_H

#include "common.h"
#include <vector>
#include <algorithm>

class BStarTree {
public:
    explicit BStarTree(int order)
        : d_(order), MAX_KEYS_(2 * order), MIN_KEYS_(order),
          root_(new Node(true)) {}
    ~BStarTree() { destroy(root_); }
    BStarTree(const BStarTree&)            = delete;
    BStarTree& operator=(const BStarTree&) = delete;
    bool search(int key, RID& out) const { return search_node(root_, key, out); }
    void insert(int key, RID rid) {
        InsertResult res = insert_into(root_, key, rid, /*parent=*/nullptr, /*idx=*/0);
        if (res.split) {
            Node* nr = new Node(false);
            nr->keys.push_back(res.up_key);
            nr->rids.push_back(res.up_rid);
            nr->children.push_back(root_);
            nr->children.push_back(res.right);
            root_ = nr;
        } else if (res.split23) {
            // 2-to-3 root split.
            Node* nr = new Node(false);
            nr->keys  = { res.up_key, res.up_key2 };
            nr->rids  = { res.up_rid, res.up_rid2 };
            nr->children = { root_, res.middle, res.right };
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
        range_walk(root_, lo, hi, visit);
    }

    long long split_count()        const { return splits_; }
    long long redistribute_count() const { return redistributes_; }
    long long split_23_count()     const { return splits_23_; }
    long long node_count()         const { return count_nodes(root_); }
    long long key_count()          const { return count_keys(root_); }
    int       height()             const { return height_of(root_); }
    double    utilization()        const {
        long long n = node_count();
        if (!n) return 0.0;
        return double(key_count()) / double(n * MAX_KEYS_);
    }
private:
    struct Node {
        bool is_leaf;
        std::vector<int>   keys;
        std::vector<RID>   rids;
        std::vector<Node*> children;
        explicit Node(bool leaf) : is_leaf(leaf) {}
    };
    int d_, MAX_KEYS_, MIN_KEYS_;
    Node* root_;
    long long splits_        = 0;
    long long splits_23_     = 0;
    long long redistributes_ = 0;
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
        if (i < n->keys.size() && key == n->keys[i]) { out = n->rids[i]; return true; }
        if (n->is_leaf) return false;
        return search_node(n->children[i], key, out);
    }
    struct InsertResult {
        bool  split    = false;
        bool  split23  = false;
        int   up_key   = 0;
        RID   up_rid   = 0;
        Node* right    = nullptr;     // for split
        int   up_key2  = 0;           // for split23
        RID   up_rid2  = 0;
        Node* middle   = nullptr;     // for split23
    };
    InsertResult insert_into(Node* n, int key, RID rid, Node* parent, size_t idx) {
        size_t i = 0;
        while (i < n->keys.size() && key > n->keys[i]) ++i;
        if (i < n->keys.size() && key == n->keys[i]) {
            n->rids[i] = rid; return {};
        }
        if (n->is_leaf) {
            n->keys.insert(n->keys.begin() + i, key);
            n->rids.insert(n->rids.begin() + i, rid);
        } else {
            InsertResult c = insert_into(n->children[i], key, rid, n, i);
            if (c.split) {
                n->keys.insert(n->keys.begin() + i, c.up_key);
                n->rids.insert(n->rids.begin() + i, c.up_rid);
                n->children.insert(n->children.begin() + i + 1, c.right);
            } else if (c.split23) {
                n->keys.insert(n->keys.begin() + i,     c.up_key2);
                n->rids.insert(n->rids.begin() + i,     c.up_rid2);
                n->keys.insert(n->keys.begin() + i,     c.up_key);
                n->rids.insert(n->rids.begin() + i,     c.up_rid);
                n->children.insert(n->children.begin() + i + 1, c.middle);
                n->children.insert(n->children.begin() + i + 2, c.right);
            } else {
            }
        }
        if ((int)n->keys.size() <= MAX_KEYS_) return {};
        if (parent && try_redistribute(parent, idx)) {
            ++redistributes_;
            return {};
        }
        if (parent) {
            split_23_propagate(parent, idx);
            return {};
        }
        return split_1_to_2(n);
    }
    bool try_redistribute(Node* parent, size_t idx) {
        Node* c = parent->children[idx];
        if (idx > 0) {
            Node* L = parent->children[idx - 1];
            if ((int)L->keys.size() < MAX_KEYS_) {
                redistribute_with_left(parent, idx, L, c);
                return true;
            }
        }
        if (idx + 1 < parent->children.size()) {
            Node* R = parent->children[idx + 1];
            if ((int)R->keys.size() < MAX_KEYS_) {
                redistribute_with_right(parent, idx, c, R);
                return true;
            }
        }
        return false;
    }
    void redistribute_with_left(Node* parent, size_t idx, Node* L, Node* c) {
        size_t sep = idx - 1;
        // Build the combined sequence:  L | sep | c
        std::vector<int>   ak;  std::vector<RID> ar;
        ak.insert(ak.end(), L->keys.begin(), L->keys.end());
        ar.insert(ar.end(), L->rids.begin(), L->rids.end());
        ak.push_back(parent->keys[sep]); ar.push_back(parent->rids[sep]);
        ak.insert(ak.end(), c->keys.begin(), c->keys.end());
        ar.insert(ar.end(), c->rids.begin(), c->rids.end());
        std::vector<Node*> ach;
        if (!L->is_leaf) {
            ach.insert(ach.end(), L->children.begin(), L->children.end());
            ach.insert(ach.end(), c->children.begin(), c->children.end());
        }
        size_t mid = ak.size() / 2;
        L->keys.assign(ak.begin(), ak.begin() + mid);
        L->rids.assign(ar.begin(), ar.begin() + mid);
        parent->keys[sep] = ak[mid];
        parent->rids[sep] = ar[mid];
        c->keys.assign(ak.begin() + mid + 1, ak.end());
        c->rids.assign(ar.begin() + mid + 1, ar.end());
        if (!L->is_leaf) {
            // Children: left gets first (mid+1) children, c gets the rest.
            L->children.assign(ach.begin(), ach.begin() + mid + 1);
            c->children.assign(ach.begin() + mid + 1, ach.end());
        }
    }
    void redistribute_with_right(Node* parent, size_t idx, Node* c, Node* R) {
        size_t sep = idx;
        std::vector<int> ak; std::vector<RID> ar;
        ak.insert(ak.end(), c->keys.begin(), c->keys.end());
        ar.insert(ar.end(), c->rids.begin(), c->rids.end());
        ak.push_back(parent->keys[sep]); ar.push_back(parent->rids[sep]);
        ak.insert(ak.end(), R->keys.begin(), R->keys.end());
        ar.insert(ar.end(), R->rids.begin(), R->rids.end());

        std::vector<Node*> ach;
        if (!c->is_leaf) {
            ach.insert(ach.end(), c->children.begin(), c->children.end());
            ach.insert(ach.end(), R->children.begin(), R->children.end());
        }
        size_t mid = ak.size() / 2;
        c->keys.assign(ak.begin(), ak.begin() + mid);
        c->rids.assign(ar.begin(), ar.begin() + mid);
        parent->keys[sep] = ak[mid];
        parent->rids[sep] = ar[mid];
        R->keys.assign(ak.begin() + mid + 1, ak.end());
        R->rids.assign(ar.begin() + mid + 1, ar.end());
        if (!c->is_leaf) {
            c->children.assign(ach.begin(), ach.begin() + mid + 1);
            R->children.assign(ach.begin() + mid + 1, ach.end());
        }
    }
    void split_23_propagate(Node* parent, size_t idx) {
        ++splits_23_;
        Node *A, *B;
        size_t sep_idx;
        bool left_partner = (idx > 0);
        if (left_partner) {
            A = parent->children[idx - 1];
            B = parent->children[idx];
            sep_idx = idx - 1;
        } else {
            A = parent->children[idx];
            B = parent->children[idx + 1];
            sep_idx = idx;
        }
        std::vector<int> ak; std::vector<RID> ar;
        ak.insert(ak.end(), A->keys.begin(), A->keys.end());
        ar.insert(ar.end(), A->rids.begin(), A->rids.end());
        ak.push_back(parent->keys[sep_idx]); ar.push_back(parent->rids[sep_idx]);
        ak.insert(ak.end(), B->keys.begin(), B->keys.end());
        ar.insert(ar.end(), B->rids.begin(), B->rids.end());
        std::vector<Node*> ach;
        if (!A->is_leaf) {
            ach.insert(ach.end(), A->children.begin(), A->children.end());
            ach.insert(ach.end(), B->children.begin(), B->children.end());
        }
        size_t total = ak.size();
        size_t s1 = total / 3;
        size_t s2 = (2 * total + 1) / 3;
        Node* M = new Node(A->is_leaf);
        std::vector<int>  Ak(ak.begin(),            ak.begin() + s1);
        std::vector<RID>  Ar(ar.begin(),            ar.begin() + s1);
        int   sep1_key = ak[s1];
        RID   sep1_rid = ar[s1];
        std::vector<int>  Mk(ak.begin() + s1 + 1,   ak.begin() + s2);
        std::vector<RID>  Mr(ar.begin() + s1 + 1,   ar.begin() + s2);
        int   sep2_key = ak[s2];
        RID   sep2_rid = ar[s2];
        std::vector<int>  Bk(ak.begin() + s2 + 1,   ak.end());
        std::vector<RID>  Br(ar.begin() + s2 + 1,   ar.end());
        A->keys = std::move(Ak); A->rids = std::move(Ar);
        M->keys = std::move(Mk); M->rids = std::move(Mr);
        B->keys = std::move(Bk); B->rids = std::move(Br);
        if (!A->is_leaf) {
            // Children get split at s1+1 and s2+1.
            std::vector<Node*> Ac(ach.begin(),              ach.begin() + s1 + 1);
            std::vector<Node*> Mc(ach.begin() + s1 + 1,     ach.begin() + s2 + 1);
            std::vector<Node*> Bc(ach.begin() + s2 + 1,     ach.end());
            A->children = std::move(Ac);
            M->children = std::move(Mc);
            B->children = std::move(Bc);
        }
        parent->keys[sep_idx] = sep1_key;
        parent->rids[sep_idx] = sep1_rid;
        parent->keys.insert(parent->keys.begin() + sep_idx + 1, sep2_key);
        parent->rids.insert(parent->rids.begin() + sep_idx + 1, sep2_rid);
        parent->children.insert(parent->children.begin() + sep_idx + 1, M);
    }
    InsertResult split_1_to_2(Node* n) {
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
            Node* succ = n->children[i + 1];
            while (!succ->is_leaf) succ = succ->children.front();
            int sk = succ->keys.front(); RID sr = succ->rids.front();
            n->keys[i] = sk; n->rids[i] = sr;
            bool ok = delete_from(n->children[i + 1], sk);
            fix_underflow(n, i + 1);
            return ok;
        }
        if (n->is_leaf) return false;
        bool ok = delete_from(n->children[i], key);
        fix_underflow(n, i);
        return ok;
    }
    void fix_underflow(Node* parent, size_t ci) {
        Node* c = parent->children[ci];
        if ((int)c->keys.size() >= MIN_KEYS_) return;
        Node* L = ci > 0 ? parent->children[ci - 1] : nullptr;
        Node* R = ci + 1 < parent->children.size() ? parent->children[ci + 1] : nullptr;
        if (L && (int)L->keys.size() > MIN_KEYS_) {
            c->keys.insert(c->keys.begin(), parent->keys[ci - 1]);
            c->rids.insert(c->rids.begin(), parent->rids[ci - 1]);
            parent->keys[ci - 1] = L->keys.back();
            parent->rids[ci - 1] = L->rids.back();
            L->keys.pop_back(); L->rids.pop_back();
            if (!L->is_leaf) {
                c->children.insert(c->children.begin(), L->children.back());
                L->children.pop_back();
            }
            return;
        }
        if (R && (int)R->keys.size() > MIN_KEYS_) {
            c->keys.push_back(parent->keys[ci]);
            c->rids.push_back(parent->rids[ci]);
            parent->keys[ci] = R->keys.front();
            parent->rids[ci] = R->rids.front();
            R->keys.erase(R->keys.begin()); R->rids.erase(R->rids.begin());
            if (!R->is_leaf) {
                c->children.push_back(R->children.front());
                R->children.erase(R->children.begin());
            }
            return;
        }
        if (L) merge_children(parent, ci - 1);
        else   merge_children(parent, ci);
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
        if (!n->is_leaf && i < n->children.size()) range_walk(n->children[i], lo, hi, visit);
    }
};
#endif // CSE321_BSTAR_TREE_H
