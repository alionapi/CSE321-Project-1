#include "common.h"
#include "btree.h"
#include "bstar_tree.h"
#include "bplus_tree.h"
#include <iostream>
#include <random>
#include <algorithm>
#include <set>
#include <cassert>
template <class Tree>
bool test_tree(const std::string& name, int d, int N) {
    Tree t(d);
    std::mt19937 rng(2024);
    std::vector<int> keys(N);
    for (int i = 0; i < N; ++i) keys[i] = i + 1;
    std::shuffle(keys.begin(), keys.end(), rng);
    for (int i = 0; i < N; ++i) t.insert(keys[i], i);
    for (int i = 0; i < N; ++i) {
        RID r;
        if (!t.search(keys[i], r)) {
            std::cerr << name << " d=" << d << ": missing key " << keys[i] << "\n";
            return false;
        }
    }
    int LO = N/4, HI = 3*N/4;
    std::vector<int> got;
    t.range_query(LO, HI, [&](int k, RID){ got.push_back(k); });
    std::sort(got.begin(), got.end());

    std::vector<int> expect;
    for (int i = LO; i <= HI; ++i) expect.push_back(i);

    if (got != expect) {
        std::cerr << name << " d=" << d << ": range query mismatch (got "
                  << got.size() << ", expected " << expect.size() << ")\n";
        return false;
    }
    std::set<int> deleted;
    for (int i = 0; i < N; i += 2) {
        if (!t.remove(keys[i])) {
            std::cerr << name << " d=" << d << ": failed to remove " << keys[i] << "\n";
            return false;
        }
        deleted.insert(keys[i]);
    }
    for (int i = 0; i < N; ++i) {
        RID r;
        bool found = t.search(keys[i], r);
        bool was_deleted = deleted.count(keys[i]) > 0;
        if (found == was_deleted) {
            std::cerr << name << " d=" << d << ": post-delete fail at " << keys[i]
                      << " (found=" << found << ", deleted=" << was_deleted << ")\n";
            return false;
        }
    }
    std::cout << "  " << name << " d=" << d << "  OK  (N=" << N << ")\n";
    return true;
}
int main() {
    bool all = true;
    for (int d : {3, 5, 10, 16}) {
        all &= test_tree<BTree>     ("BTree    ", d, 5000);
        all &= test_tree<BStarTree> ("BStarTree", d, 5000);
        all &= test_tree<BPlusTree> ("BPlusTree", d, 5000);
    }
    std::cout << (all ? "ALL TESTS PASSED\n" : "FAILURES\n");
    return all ? 0 : 1;
}