#include "common.h"
#include "csv_loader.h"
#include "btree.h"
#include "bstar_tree.h"
#include "bplus_tree.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <fstream>

using clk = std::chrono::high_resolution_clock;
template <class F>
double time_ms(F&& f) {
    auto t0 = clk::now();
    f();
    auto t1 = clk::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void print_section(const std::string& s) {
    std::cout << "\n=========================================================\n";
    std::cout << "  " << s << "\n";
    std::cout << "=========================================================\n";
}
struct InsertStats {
    double   time_ms       = 0;
    long long splits       = 0;
    long long splits_23    = 0;
    long long redistribute = 0;
    long long nodes        = 0;
    int       height       = 0;
    double    utilization  = 0;
};
InsertStats run_insert_btree(const std::vector<Record>& recs, int d) {
    BTree t(d);
    InsertStats s;
    s.time_ms = time_ms([&]() {
        for (RID i = 0; i < (RID)recs.size(); ++i)
            t.insert(recs[i].student_id, i);
    });
    s.splits      = t.split_count();
    s.nodes       = t.node_count();
    s.height      = t.height();
    s.utilization = t.utilization();
    return s;
}
InsertStats run_insert_bstar(const std::vector<Record>& recs, int d) {
    BStarTree t(d);
    InsertStats s;
    s.time_ms = time_ms([&]() {
        for (RID i = 0; i < (RID)recs.size(); ++i)
            t.insert(recs[i].student_id, i);
    });
    s.splits       = t.split_count();
    s.splits_23    = t.split_23_count();
    s.redistribute = t.redistribute_count();
    s.nodes        = t.node_count();
    s.height       = t.height();
    s.utilization  = t.utilization();
    return s;
}
InsertStats run_insert_bplus(const std::vector<Record>& recs, int d) {
    BPlusTree t(d);
    InsertStats s;
    s.time_ms = time_ms([&]() {
        for (RID i = 0; i < (RID)recs.size(); ++i)
            t.insert(recs[i].student_id, i);
    });
    s.splits      = t.split_count();
    s.nodes       = t.node_count();
    s.height      = t.height();
    s.utilization = t.utilization();
    return s;
}
void experiment_insertion(const std::vector<Record>& recs) {
    print_section("Experiment 1 - Insertion (100k records, varying order d)");
    std::cout << std::left
              << std::setw(8)  << "Tree"
              << std::setw(5)  << "d"
              << std::setw(14) << "Time(ms)"
              << std::setw(12) << "Splits"
              << std::setw(12) << "2to3-Splits"
              << std::setw(14) << "Redistrib"
              << std::setw(10) << "Nodes"
              << std::setw(8)  << "Height"
              << std::setw(8)  << "Util"
              << "\n";
    auto print_row = [](const std::string& name, int d, const InsertStats& s){
        std::cout << std::left
                  << std::setw(8)  << name
                  << std::setw(5)  << d
                  << std::setw(14) << std::fixed << std::setprecision(2) << s.time_ms
                  << std::setw(12) << s.splits
                  << std::setw(12) << s.splits_23
                  << std::setw(14) << s.redistribute
                  << std::setw(10) << s.nodes
                  << std::setw(8)  << s.height
                  << std::setw(8)  << std::setprecision(3) << s.utilization
                  << "\n";
    };
    for (int d : {3, 5, 10}) {
        print_row("B",     d, run_insert_btree(recs, d));
        print_row("B*",    d, run_insert_bstar(recs, d));
        print_row("B+",    d, run_insert_bplus(recs, d));
        std::cout << "---------------------------------------------------------\n";
    }
}
void experiment_search(const std::vector<Record>& recs, int d_for_search) {
    print_section("Experiment 2 - Point Search (10,000 random keys, d=" +
                  std::to_string(d_for_search) + ")");
    BTree     t1(d_for_search);
    BStarTree t2(d_for_search);
    BPlusTree t3(d_for_search);
    for (RID i = 0; i < (RID)recs.size(); ++i) {
        t1.insert(recs[i].student_id, i);
        t2.insert(recs[i].student_id, i);
        t3.insert(recs[i].student_id, i);
    }
    std::mt19937 rng(2026);
    std::uniform_int_distribution<int> pick(0, (int)recs.size() - 1);
    const int N = 10000;
    std::vector<int> queries; queries.reserve(N);
    for (int i = 0; i < N; ++i) queries.push_back(recs[pick(rng)].student_id);

    auto bench = [&](auto& tree, const std::string& name) {
        long long hits = 0;
        double total_ms = time_ms([&](){
            RID r;
            for (int k : queries) if (tree.search(k, r)) ++hits;
        });
        std::cout << std::left << std::setw(8) << name
                  << "total=" << std::fixed << std::setprecision(2) << total_ms
                  << " ms,  mean=" << std::setprecision(4) << (total_ms / N) << " ms/op"
                  << ",  hits=" << hits << "/" << N << "\n";
    };
    bench(t1, "B");
    bench(t2, "B*");
    bench(t3, "B+");
}
void experiment_range(const std::vector<Record>& recs, int d) {
    int min_id = recs.front().student_id, max_id = min_id;
    for (const auto& r : recs) {
        if (r.student_id < min_id) min_id = r.student_id;
        if (r.student_id > max_id) max_id = r.student_id;
    }
    long long span = max_id - min_id;
    int LO = min_id;
    int HI = (int)(min_id + span / 7);
    print_section("Experiment 3 - Range Query (avg GPA & height of MALE "
                  "students with ID in [" + std::to_string(LO) + ", "
                  + std::to_string(HI) + "], d=" + std::to_string(d) + ")");
    BTree     t1(d);
    BStarTree t2(d);
    BPlusTree t3(d);
    for (RID i = 0; i < (RID)recs.size(); ++i) {
        t1.insert(recs[i].student_id, i);
        t2.insert(recs[i].student_id, i);
        t3.insert(recs[i].student_id, i);
    }
    auto run_range = [&](auto& tree, const std::string& name){
        long long count = 0;
        double sum_gpa = 0, sum_height = 0;
        double t_ms = time_ms([&](){
            tree.range_query(LO, HI, [&](int /*key*/, RID rid){
                const Record& r = recs[rid];
                if (r.gender == "Male") {
                    ++count;
                    sum_gpa    += r.gpa;
                    sum_height += r.height;
                }
            });
        });
        double avg_gpa    = count ? sum_gpa    / count : 0.0;
        double avg_height = count ? sum_height / count : 0.0;
        std::cout << std::left << std::setw(8) << name
                  << "time=" << std::fixed << std::setprecision(3) << t_ms << " ms,  "
                  << "matched=" << count
                  << ",  avgGPA=" << std::setprecision(3) << avg_gpa
                  << ",  avgHeight=" << std::setprecision(2) << avg_height << " cm\n";
    };
    run_range(t1, "B");
    run_range(t2, "B*");
    run_range(t3, "B+");
}
template <class Tree>
bool integrity_check(Tree& tree,
                     const std::vector<int>& expect_present,
                     const std::vector<int>& expect_absent) {
    RID dummy;
    for (int k : expect_present) if (!tree.search(k, dummy)) return false;
    for (int k : expect_absent)  if ( tree.search(k, dummy)) return false;
    return true;
}

void experiment_delete(const std::vector<Record>& recs, int d) {
    print_section("Experiment 4 - Deletion (d=" + std::to_string(d) + ")");
    BTree     t1(d);
    BStarTree t2(d);
    BPlusTree t3(d);
    for (RID i = 0; i < (RID)recs.size(); ++i) {
        t1.insert(recs[i].student_id, i);
        t2.insert(recs[i].student_id, i);
        t3.insert(recs[i].student_id, i);
    }
    auto make_delete_set = [&](size_t n, std::mt19937& rng) {
        std::vector<int> idx(recs.size());
        for (size_t i = 0; i < recs.size(); ++i) idx[i] = (int)i;
        std::shuffle(idx.begin(), idx.end(), rng);
        if (n > idx.size()) n = idx.size();
        idx.resize(n);
        std::vector<int> keys; keys.reserve(n);
        for (int i : idx) keys.push_back(recs[i].student_id);
        return keys;
    };
    std::mt19937 rng(7);
    auto del2k = make_delete_set(2000, rng);
    auto bench_delete = [&](auto& tree, const std::string& name,
                            const std::vector<int>& keys) {
        long long removed = 0;
        double t_ms = time_ms([&](){
            for (int k : keys) if (tree.remove(k)) ++removed;
        });
        std::cout << std::left << std::setw(8) << name
                  << "deleted " << removed << "/" << keys.size()
                  << "  in " << std::fixed << std::setprecision(2) << t_ms << " ms\n";
    };
    std::cout << "[Phase A] Delete 2,000 random records:\n";
    bench_delete(t1, "B",  del2k);
    bench_delete(t2, "B*", del2k);
    bench_delete(t3, "B+", del2k);
    std::vector<int> absent(del2k.begin(), del2k.begin() + 50);
    std::unordered_set<int> deleted_set(del2k.begin(), del2k.end());
    std::vector<int> present;
    for (const auto& r : recs) {
        if (deleted_set.find(r.student_id) == deleted_set.end()) {
            present.push_back(r.student_id);
            if (present.size() >= 50) break;
        }
    }
    std::cout << "  Integrity: B="  << (integrity_check(t1, present, absent) ? "OK" : "FAIL")
              <<        ",  B*=" << (integrity_check(t2, present, absent) ? "OK" : "FAIL")
              <<        ",  B+=" << (integrity_check(t3, present, absent) ? "OK" : "FAIL")
              << "\n";

    auto fresh_run = [&](double pct){
        size_t n = (size_t)(pct * recs.size());
        std::mt19937 rng2(42 + (int)(pct * 100));
        auto keys = make_delete_set(n, rng2);
        BTree     a(d); BStarTree b(d); BPlusTree c(d);
        for (RID i = 0; i < (RID)recs.size(); ++i) {
            a.insert(recs[i].student_id, i);
            b.insert(recs[i].student_id, i);
            c.insert(recs[i].student_id, i);
        }
        std::cout << "[Phase B] Delete " << (int)(pct*100) << "% (" << n << " records):\n";
        bench_delete(a, "B",  keys);
        bench_delete(b, "B*", keys);
        bench_delete(c, "B+", keys);
    };
    fresh_run(0.10);
    fresh_run(0.20);
}
int main(int argc, char** argv) {
    std::string csv_path = "student.csv";
    if (argc >= 2) csv_path = argv[1];
    std::cout << "Loading CSV from: " << csv_path << "\n";
    auto recs = load_csv(csv_path);
    std::cout << "Loaded " << recs.size() << " records.\n";
    if (recs.empty()) {
        std::cerr << "Empty dataset — aborting.\n";
        return 1;
    }
    experiment_insertion(recs);
    experiment_search(recs, /*d=*/5);
    experiment_range  (recs, /*d=*/5);
    experiment_delete (recs, /*d=*/5);

    std::cout << "\nDone.\n";
    return 0;
}
