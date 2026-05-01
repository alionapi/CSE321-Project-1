# CSE321-Project-1
## B-tree, B*-tree, B+-tree
Implementation and Analysis of B-tree Index Structures

## Project layout
```
.
├── CMakeLists.txt
├── README.md
├── student.csv                 # 100,000 records (provided by instructor)
│                               
└── src/
    ├── common.h              # Record + RID definitions
    ├── csv_loader.h          # CSV reader (auto-detects header row)
    ├── btree.h               # Classical B-tree of order d
    ├── bstar_tree.h          # B*-tree (redistribution before split, 2-to-3 split)
    ├── bplus_tree.h          # B+-tree (data only at leaves, leaves linked)
    ├── main.cpp              # Experiment driver (insertion / search / range / delete)
    ├── test_correctness.cpp  # Quick sanity tests across orders d
    └── gen_data.cpp          # Optional synthetic-data generator
```

## Build & run

### Compile from the command line (any platform)
```bash
g++ -std=c++17 -O2 -Wall -Wextra -o btree_project src/main.cpp
g++ -std=c++17 -O2 -Wall -Wextra -o test_correctness src/test_correctness.cpp
g++ -std=c++17 -O2 -o gen_data src/gen_data.cpp        # only if you need synthetic data
```

### Build with CLion (macOS, Apple-clang)
1. **File → Open…** and select this folder. CLion auto-loads `CMakeLists.txt`.
2. Pick the **Release** build profile (top-right toolbar). The default
   `CMAKE_BUILD_TYPE` is already set to `Release`, but CLion may also create
   a `Debug` profile — make sure timing experiments are run from `Release`.
3. The CMake file defines three targets: `btree_project`, `test_correctness`,
   and `gen_data`. Pick `btree_project` from the run-configuration dropdown.
4. **Run → Edit Configurations…** — set the working directory to the project
   root (so the relative path `data/student.csv` resolves) and add
   `data/student.csv` as a program argument (or the path to the TA-provided
   CSV).
5. Hit **Run** ▶ (or **⇧F10**).

### Tested toolchain
- macOS 14.x, Apple clang 15.0 (`c++17`).
- Also works with gcc-12 / gcc-13 on Ubuntu.
- No third-party dependencies — only the C++ standard library
  (per the project's "no library tree implementations" rule).

## Input format

`data/students.csv` — one student per line, comma-separated:

```
StudentID,Name,Gender,GPA,Height,Weight
202000123,Taejoon Han,Male,3.85,178.4,72.3
202100456,Jisoo Kim,Female,3.42,164.7,55.1
...
```

- A header row is auto-detected (whether the first column parses as an int).
- IDs are loaded into a `std::vector<Record>` as the **simplified storage
  model**; the array index is used as the RID.
- IDs are assumed to be unique (Student IDs); if duplicates appear the latest
  RID for that key is kept.

## Output format

Each experiment prints a banner and a small results table. Sample run:

```
Loading CSV from: data/student.csv
Loaded 100000 records.

=========================================================
  Experiment 1 - Insertion (100k records, varying order d)
=========================================================
Tree    d    Time(ms)      Splits      2to3-Splits Redistrib     Nodes     Height  Util
B       3    55.89         24530       0           0             24537     7       0.679
B*      3    46.41         6           19794       35638         19807     7       0.841
B+      3    36.94         28776       0           0             28784     8       0.713
...

=========================================================
  Experiment 2 - Point Search (10,000 random keys, d=5)
=========================================================
B       total=2.16 ms,  mean=0.0002 ms/op,  hits=10000/10000
B*      total=2.02 ms,  mean=0.0002 ms/op,  hits=10000/10000
B+      total=2.41 ms,  mean=0.0002 ms/op,  hits=10000/10000

(... range query and deletion sections follow ...)
```

You can pipe the output to a file for the report:
```bash
./btree_project data/student.csv | tee results.txt
```

## Reproducing the experiments

1. **Place the TA-provided CSV at `data/student.csv`.**
   If you don't have it yet, generate a synthetic 100k-row file:
   ```bash
   ./gen_data data/student.csv 100000
   ```
   (Synthetic IDs are 9 digits in the form `2020xxxxx … 2026xxxxx`; the
   experiment driver auto-derives the range-query bounds from the dataset
   so the same code works for both 8- and 9-digit IDs.)

2. **Run correctness tests first** (highly recommended):
   ```bash
   ./test_correctness
   ```
   This inserts/searches/range-queries/deletes 5,000 keys against each tree
   at orders d ∈ {3, 5, 10, 16} and aborts if anything is inconsistent.

3. **Run the full experiment**:
   ```bash
   ./btree_project data/student.csv
   ```
   Total runtime is a few seconds on a recent laptop. The driver covers all
   four mandatory workloads from §3.1 of the spec:
   - Insertion with d ∈ {3, 5, 10}
   - Point search of 10,000 random keys
   - Range query "average GPA + height of male students in lowest-cohort range"
   - Deletion of 2,000 random records, then 10% and 20% slices on fresh trees,
     each followed by an integrity check (search the deleted keys and a sample
     of surviving keys)

## Implementation notes

### B-tree (`btree.h`)
- Order d ⇒ 2d max keys, d min keys per non-root node.
- Internal nodes carry `(key, RID)` pairs; search may terminate at any level
  (allowed by the spec).
- Insert is recursive with split-on-overflow; the middle (key, RID) is
  promoted to the parent.
- Delete uses borrow-from-sibling first, then merge.

### B*-tree (`bstar_tree.h`)
- Same node shape as the B-tree.
- On overflow the algorithm first tries **redistribution** with an adjacent
  sibling. If the sibling is also full, it performs a **2-to-3 split**:
  two full nodes (4d+1 keys total including the parent separator) become
  three nodes with ~⅔ fill, promoting two new separators to the parent.
- This raises the asymptotic minimum fill factor from 50% (B-tree) to ~67%.

### B+-tree (`bplus_tree.h`)
- Internal nodes hold only keys + child pointers (no RIDs).
- All `(key, RID)` pairs live in leaves.
- Leaves are linked left-to-right via a `next` pointer; range queries jump
  to the starting leaf and walk forward, never re-entering internal levels.
- Search always descends to a leaf (per the spec).

## Assumptions
- Student IDs are unique. (If the actual dataset has duplicates, only the
  most recent RID for that key is retained.)
- Records fit in memory (100k records is well under any plausible limit).
- Timing uses `std::chrono::high_resolution_clock`. Numbers are wall-clock
  with no warm-up loop; they are stable to ±~10% across runs on the same
  machine and intended for relative comparison rather than absolute
  benchmarking.

## Limitations / honest disclosures
- The B*-tree's *delete* path uses the standard borrow-then-merge rule
  rather than a fully symmetric 3-to-2 inverse of the 2-to-3 split. This
  preserves correctness and the spec's structural-integrity requirement,
  but the tree's fill factor under heavy delete workloads will drop toward
  the same lower bound (~50%) as a plain B-tree. Insertion behaviour
  (which is what the project explicitly evaluates) is fully B*.
- The driver runs each experiment once. For more stable timing the full
  pipeline should be run several times and averaged; in practice the
  runtimes reported here are stable enough to support the report's
  qualitative conclusions.
