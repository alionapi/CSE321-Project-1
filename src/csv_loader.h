#ifndef CSE321_CSV_LOADER_H
#define CSE321_CSV_LOADER_H

#include "common.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

inline std::vector<Record> load_csv(const std::string& path) {
    std::ifstream fin(path);
    if (!fin.is_open()) {
        throw std::runtime_error("Could not open CSV file: " + path);
    }
    std::vector<Record> records;
    records.reserve(110000);
    std::string line;
    bool first_line = true;
    while (std::getline(fin, line)) {
        rtrim_cr(line);
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string tok;
        std::vector<std::string> cols;
        while (std::getline(ss, tok, ',')) {
            cols.push_back(tok);
        }
        if (cols.size() < 6) continue;  // malformed row -> skip
        if (first_line) {
            first_line = false;
            try {
                std::stoi(cols[0]);
            } catch (...) {
                continue;
            }
        }
        Record r;
        try {
            r.student_id = std::stoi(cols[0]);
            r.name       = cols[1];
            r.gender     = cols[2];
            r.gpa        = std::stod(cols[3]);
            r.height     = std::stod(cols[4]);
            r.weight     = std::stod(cols[5]);
        } catch (...) {
            // Bad row, just skip it.
            continue;
        }
        records.push_back(std::move(r));
    }
    return records;
}
#endif // CSE321_CSV_LOADER_H
