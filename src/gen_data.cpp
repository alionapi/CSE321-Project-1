#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

int main(int argc, char** argv) {
    std::string out = (argc >= 2) ? argv[1] : "student.csv";
    int n = (argc >= 3) ? std::stoi(argv[2]) : 100000;

    std::ofstream f(out);
    f << "StudentID,Name,Gender,GPA,Height,Weight\n";

    std::mt19937 rng(123);
    std::uniform_int_distribution<int> year(2020, 2026);
    std::uniform_int_distribution<int> tail(0, 99999);
    std::uniform_int_distribution<int> g(0, 1);
    std::uniform_real_distribution<double> gpa(2.00, 4.30);
    std::uniform_real_distribution<double> hM(160.0, 195.0);
    std::uniform_real_distribution<double> hF(150.0, 180.0);
    std::uniform_real_distribution<double> wM(55.0, 100.0);
    std::uniform_real_distribution<double> wF(45.0, 80.0);

    const char* first[] = {"Taejoon","Minji","Jisoo","Hyunwoo","Seoyeon",
                           "Jaewon","Yuna","Junho","Hana","Sungho",
                           "Mina","Daehyun","Soyoung","Kihoon","Eunji"};
    const char* last [] = {"Han","Kim","Lee","Park","Choi","Jung","Yoon",
                           "Cho","Kang","Lim","Shin","Oh","Bae","Hwang"};
    std::vector<int> ids; ids.reserve(n);
    std::set<int> seen;
    while ((int)ids.size() < n) {
        int id = year(rng) * 100000 + tail(rng);
        if (seen.insert(id).second) ids.push_back(id);
    }
    std::shuffle(ids.begin(), ids.end(), rng);
    f.setf(std::ios::fixed); f.precision(2);
    for (int id : ids) {
        bool male = g(rng);
        const char* fn = first[std::uniform_int_distribution<int>(0,14)(rng)];
        const char* ln = last [std::uniform_int_distribution<int>(0,13)(rng)];
        double gp = gpa(rng);
        double h  = male ? hM(rng) : hF(rng);
        double w  = male ? wM(rng) : wF(rng);
        f << id << "," << fn << " " << ln << ","
          << (male ? "Male" : "Female") << ","
          << gp << "," << h << "," << w << "\n";
    }
    std::cerr << "Wrote " << ids.size() << " records to " << out << "\n";
}