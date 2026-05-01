#ifndef CSE321_COMMON_H
#define CSE321_COMMON_H

#include <string>
#include <vector>
#include <cstdint>
struct Record {
    int         student_id;
    std::string name;
    std::string gender;
    double      gpa;
    double      height;
    double      weight;
};
using RID = int;
inline void rtrim_cr(std::string& s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' ||
                          s.back() == ' '  || s.back() == '\t')) {
        s.pop_back();
    }
}
#endif // CSE321_COMMON_H
