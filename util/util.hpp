#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>
#include <random>
#include <openssl/rand.h>

namespace util {

static uint64_t timing(std::function<void()> fn) {
    const auto start = std::chrono::high_resolution_clock::now();
    fn();
    const auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// Pins the current thread to core `core_id`.
static void set_cpu_affinity(const uint32_t core_id) __attribute__((unused));
static void set_cpu_affinity(const uint32_t core_id) {
#ifdef __linux__
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core_id % std::thread::hardware_concurrency(), &mask);
    const int result =
        pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
#else
    (void)core_id;
    std::cout << "we only support thread pinning under Linux" << std::endl;
#endif
}

uint64_t generate_random() {
    uint64_t value;
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&value), sizeof(value)) != 1) {
        std::cerr << "RAND_bytes failed" << std::endl;
        abort();
    }
    return value;
}

thread_local std::mt19937_64 rng(std::random_device{}());
uint64_t generate_random_fast() {
    return rng();
}

const uint64_t string_rng_seed = 1000;
static std::mt19937_64 string_rng(string_rng_seed);
static inline void generate_random_string(uint8_t *str, uint32_t len) {
    uint32_t i;
    uint64_t *word_str = reinterpret_cast<uint64_t *>(str);
    for (i = 0; i + 8 <= len; i += 8)
        word_str[i / 8] = std::max<uint64_t>(string_rng(), 1ULL);
    for (; i < len; i++)
        str[i] = std::max<uint8_t>(string_rng() % 256, 1U);
    str[len] = '\0';
}

uint64_t* read_vals(const std::string& filename, uint64_t& nvals) {
    std::ifstream in_file(filename, std::ios::binary);
    if (!in_file) {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
        return nullptr;
    }
    in_file.read(reinterpret_cast<char*>(&nvals), sizeof(nvals)); // Read the count
    uint64_t* vals = (uint64_t*)malloc(nvals * sizeof(uint64_t));
    if (!vals) {
        std::cerr << "Memory allocation failed!" << std::endl;
        return nullptr;
    }
    in_file.read(reinterpret_cast<char*>(vals), nvals * sizeof(uint64_t));
    in_file.close();
    return vals;
}

void dump_vals(const std::string& filename, const uint64_t* vals, uint64_t nvals) {
    std::ofstream out_file(filename, std::ios::binary);
    if (!out_file) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }
    out_file.write(reinterpret_cast<const char*>(&nvals), sizeof(nvals)); // Store the count
    out_file.write(reinterpret_cast<const char*>(vals), nvals * sizeof(uint64_t));
    out_file.close();
}

void print_progress_bar(int current, int total, int bar_width = 50) {
    float progress = static_cast<float>(current) / total;
    int pos = static_cast<int>(bar_width * progress);

    std::cerr << "\r[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cerr << "=";
        else if (i == pos) std::cerr << ">";
        else std::cerr << " ";
    }
    std::cerr << "]" << int(progress * 100.0) << "%";
    std::cerr.flush();
}

std::vector<uint64_t> load_data(const std::string& filename) {
    std::vector<uint64_t> data;
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "unable to open " << filename << std::endl;
        exit(EXIT_FAILURE);
    }
    uint64_t size;
    in.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
    data.resize(size);
    in.read(reinterpret_cast<char*>(data.data()), size * sizeof(uint64_t));
    in.close();
    
    return data;
}

}   // namespace util