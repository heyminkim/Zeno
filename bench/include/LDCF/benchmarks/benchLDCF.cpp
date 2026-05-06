#include <cstddef>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <random> 
#include "LDCF.hpp" 

std::vector<std::string> read_sequences_from_fq(const std::string& filename) {
    std::vector<std::string> sequences;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open the file " << filename << std::endl;
        return sequences;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line[0] == '@') {
            std::getline(file, line); // Sequence line
            sequences.push_back(line);
            std::getline(file, line); // Skip '+'
            std::getline(file, line); // Skip quality line
        }
    }
    file.close();
    // print number of sequences and first sequence
    return sequences;
};


// function for generation random strings
std::vector<std::string> generate_random_strings(std::size_t num_strings, std::size_t string_length) {
    std::vector<std::string> strings;
    strings.reserve(num_strings);
    const char charset[] = "ACGT";
    std::default_random_engine rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, 3);

    for (std::size_t i = 0; i < num_strings; ++i) {
        std::string str(string_length, 0);
        for (std::size_t j = 0; j < string_length; ++j) {
            str[j] = charset[dist(rng)];
        }
        strings.push_back(str);
    }
    return strings;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <string_length> <false_positive_rate> <expected_levels>" << std::endl;
        return 1;
    }

    std::string file1 = "../benchmarks/reads_1.fq";
    std::string file2 = "../benchmarks/reads_2.fq";
    std::size_t string_length = std::stoul(argv[1]);
    double false_positive_rate = std::stod(argv[2]);
    std::size_t expected_levels = std::stoul(argv[3]);


    // read data from files
    auto sequences1 = read_sequences_from_fq(file1);
    auto sequences2 = read_sequences_from_fq(file2);
    
    // bind all seq
    std::vector<std::string> all_sequences = sequences1;
    all_sequences.insert(all_sequences.end(), sequences2.begin(), sequences2.end());

    // put all sequences in one string
    std::string all_sequences_str;
    for (const auto& seq : all_sequences) {
        all_sequences_str += seq;
    }

    // now create a vector of all substrings of length `string_length`
    std::vector<std::string> all_substrings;
    all_substrings.reserve(all_sequences_str.size() / string_length);
    for (std::size_t i = 0; i < all_sequences_str.size(); i += string_length) {
        all_substrings.push_back(all_sequences_str.substr(i, string_length));
    }

    // init LogarithmicDynamicCuckooFilter
    LogarithmicDynamicCuckooFilter ldcf(false_positive_rate, all_sequences.size(), expected_levels);

    // time clock
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& seq : all_substrings) {
        ldcf.insert(seq);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ldcf_insert_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // test false positives
    std::unordered_map<std::string, bool> string_map;
    string_map.reserve(all_substrings.size());
    for (const auto& seq : all_substrings) {
        string_map.emplace(seq, true);
    }

    std::size_t ldcf_false_positives = 0;

    // random strings for false positives
    auto false_strings = generate_random_strings(all_substrings.size(), string_length);
    std::size_t false_positive_oppotunities = 0;
    start = std::chrono::high_resolution_clock::now();
    for (const auto& seq : false_strings) {
        if (ldcf.contains(seq) && string_map.find(seq) == string_map.end()) {
            ldcf_false_positives++;
        }
        if (string_map.find(seq) == string_map.end()) {
            false_positive_oppotunities++;
        }
    }

    end = std::chrono::high_resolution_clock::now();
    auto ldcf_check_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double ldcf_fp_rate = (double)ldcf_false_positives / false_positive_oppotunities;
    
    // check if results.txt exists
    std::ofstream results("results.txt", std::ios::app);
    if (!results.is_open()) {
        // if not, create it
        results.open("results.txt");
    }
    // write results to file
    results << "LDCF Insert Time per entry: " << (double)ldcf_insert_time / all_substrings.size() << " ms\n";
    results << "LDCF Check Time per entry: " << (double)ldcf_check_time / false_strings.size() << " ms\n";
    results << "LDCF False Positive Rate: " << ldcf_fp_rate << "\n";
    results << "Number of inserted strings: " << all_substrings.size() << "\n";

    return 0;
}
