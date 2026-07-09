#include <cmath>
#include <cstdlib>
#include <string>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <thread>
#include <vector>
#include <openssl/rand.h>

#include <wiredtiger.h>

#include "util.hpp"
#include "base.hpp"

#include "zenofilter_template.hpp"
#include "zenofiltervm_template.hpp"
#include "aleph_template.hpp"

#include "../util/cxxopts.hpp"

using steady = std::chrono::steady_clock;
using namespace std::chrono;

static const char *wt_home = "./wt_database_home";
const uint32_t max_schema_len = 128;
const uint32_t max_conn_config_len = 128;
const int default_key_len = 8, default_val_len = 504;
const double default_memory_to_disk_ratio = 0.01;
static double memory_to_disk_ratio = default_memory_to_disk_ratio;
static uint64_t key_len = default_key_len, val_len = default_val_len;
static uint64_t optimizer_hack = 0;

static inline void error_check(int ret) {
    if (ret != 0) {
        std::cerr << "WiredTiger Error: " << wiredtiger_strerror(ret) << std::endl;
        exit(ret);
    }
}

static uint64_t compute_buffer_pool_size_mb(uint32_t n_keys, uint64_t filter_size) {
    return std::max((n_keys * (key_len + val_len) * memory_to_disk_ratio - filter_size) / 1024.0 / 1024.0, 1.0);
}

static inline void restart_session(WT_CONNECTION*& conn, WT_SESSION*& session, WT_CURSOR*& cursor,
                                   uint64_t buffer_size_mb) {
    error_check(conn->close(conn, NULL));
    char connection_config[max_conn_config_len];
    sprintf(connection_config, "statistics=(all),direct_io=[data],cache_size=%ldMB", buffer_size_mb);
    error_check(wiredtiger_open(wt_home, NULL, connection_config, &conn));
    error_check(conn->open_session(conn, NULL, NULL, &session));
    error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
    error_check(cursor->reset(cursor));
    return;
}

static inline void insert_kv(WT_CURSOR* cursor, const uint8_t* key, const uint8_t* value) {
    cursor->set_key(cursor, key);
    cursor->set_value(cursor, value);
    error_check(cursor->insert(cursor));
}

static inline void query_kv(WT_CURSOR* cursor, const uint8_t* key, uint8_t** value) {
    error_check(cursor->reset(cursor));
    cursor->set_key(cursor, key);
    int ret = cursor->search(cursor);
    if (ret == 0) {
        error_check(cursor->get_value(cursor, value));
    }
}

static inline std::string format_time(steady::time_point& start_time, uint64_t buffer_size) {
    auto elapsed = steady::now() - start_time;
    auto ms = duration_cast<milliseconds>(elapsed).count();
    std::string res = std::to_string(ms) + "," + std::to_string(buffer_size) + ",";
    return res;
}

std::atomic<bool> stop_measurement{false};
template <typename T>
void filter_size(T filter, std::ofstream& file) {
    auto next = steady::now();

    while (!stop_measurement.load(std::memory_order_relaxed)) {
        double size = static_cast<double>(filter->size()) / 1024.0 / 1024.0;
        file << size << ",";

        next += milliseconds(1);
        std::this_thread::sleep_until(next);
    }
}

std::atomic<uint64_t> buffer_size{0};
void measure_buffer_size(std::ofstream& file) {
    auto next = steady::now();

    while (!stop_measurement.load(std::memory_order_relaxed)) {
        
        file << buffer_size.load(std::memory_order_relaxed) << ",";

        next += milliseconds(1);
        std::this_thread::sleep_until(next);
    }
}

int main(int argc, char** argv) {
    cxxopts::Options options("ZENO", "Execute Zeno filter with parameters");
    options.add_options()
      ("q,quotient", "Length of quotient in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("f,fingerprint", "Length of fingerprint in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("e,expansion", "Number of expected expansions", cxxopts::value<uint64_t>()->default_value("1"))
      ("i,id", "Filter id", cxxopts::value<uint64_t>()->default_value("1"))
      ("d,dataset", "Dataset for evaluation", cxxopts::value<uint64_t>()->default_value("0"))
      ("fn_size", "File name to write size results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_pquery", "File name to write positive query results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_nquery", "File name to write negative query results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_fpr", "File name to write fpr results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_insert", "File name to write space/write amp results to", cxxopts::value<std::string>()->default_value("/dev/null"))
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();
    uint64_t id = result["id"].as<uint64_t>();
    uint64_t dataset_id = result["dataset"].as<uint64_t>();
    if (dataset_id > 1) {
        std::cerr << "invalid dataset" << std::endl;
        return 0;
    }

    // Files to write results
    std::string fn_size = result["fn_size"].as<std::string>();
    std::string fn_pquery = result["fn_pquery"].as<std::string>();
    std::string fn_nquery = result["fn_nquery"].as<std::string>();
    std::string fn_fpr = result["fn_fpr"].as<std::string>();
    std::string fn_insert = result["fn_insert"].as<std::string>();

    std::ofstream file_size(fn_size, std::ios::app);
    std::ofstream file_pquery(fn_pquery, std::ios::app);
    std::ofstream file_nquery(fn_nquery, std::ios::app);
    std::ofstream file_fpr(fn_fpr, std::ios::app);
    std::ofstream file_insert(fn_insert, std::ios::app);

    if (!file_size || !file_pquery || !file_nquery || !file_fpr || !file_insert) {
        std::cerr << "Error opening files." << std::endl;
        return 1;
    }

    // Parameters for benchmark
    bool record_size = (fn_size != "/dev/null");
    bool record_fpr = (fn_fpr != "/dev/null");
    bool record_insert = (fn_insert != "/dev/null");
    bool record_pquery = (fn_pquery != "/dev/null");
    bool record_nquery = (fn_nquery != "/dev/null");

    // Prepare data
    std::string directory = "../data/";
    std::string suffix = "_insert_100M_uint64";
    std::string datasets[2] = {
        "books",
        "osm_cellids"
    };
    std::string filename = directory + datasets[dataset_id] + suffix;
    auto data = util::load_data(filename);

    uint64_t data_size = data.size();
    uint64_t next_sample = data_size / 64;
    uint64_t inserted = 0;

    // For checking if with r = 2, the filter is in an intermediate epoch
    bool is_period = true;
    bool fractional = false;

    bool auto_resize = false;
    bool verbose = false;
    
    // Configure filter by given ID
    zeno_bench::Filter* filter = nullptr;
    if (id == 0) {
        // no filter configured
    }
    else if (id == 1) {
        filter = new zeno_bench::ZenoFilter(qbits, qbits + fbits, 1);
        filter->auto_resize(auto_resize);
    }
    else if (id == 2) {
        filter = new zeno_bench::ZenoFilterVM(qbits, qbits + fbits, 1);
        filter->auto_resize(auto_resize);
    }
    else if (id == 3) {
        filter = new zeno_bench::Aleph(qbits, qbits + fbits);
        filter->auto_resize(auto_resize);
    }

    std::string filter_name = filter == nullptr ? "NOFILTER" : filter->name(verbose);

    // to keep track of buffer size...
    std::string buffer_string = "NOFILTER,0,";

    if (filter) file_size << filter_name << ",";
    file_pquery << filter_name << ",";
    file_nquery << filter_name << ",";
    file_fpr << filter_name << ",";
    file_insert << filter_name << ",";

    // Evaluation results
    uint64_t insert_time = 0;
    uint64_t query_time = 0;
    // For checking insertion failure
    bool res = false;

    // For calculating the number of insertions between intervals
    uint64_t sample_interval = 1ULL << qbits;
    uint64_t prev_interval = 0;

    uint64_t npositives = 0;
    uint64_t nnegatives = 0;

    double interval = 0;
    double fraction = 0;

    // WiredTiger configurations
    WT_CONNECTION* conn;
    WT_SESSION* session;
    WT_CURSOR* cursor;
    char table_schema[max_schema_len];
    char connection_config[max_conn_config_len];

    uint64_t current_buffer_pool_size_mb = compute_buffer_pool_size_mb(data_size, 0);
    buffer_string += (std::to_string(current_buffer_pool_size_mb) + ",");

    sprintf(table_schema, "key_format=%lds,value_format=%lds", key_len, val_len);
    sprintf(connection_config, "create,statistics=(all),direct_io=[data],cache_size=%ldMB", current_buffer_pool_size_mb);

    // Remove existing db
    if (std::filesystem::exists(wt_home))
        std::filesystem::remove_all(wt_home);
    std::filesystem::create_directory(wt_home);

    error_check(wiredtiger_open(wt_home, NULL, connection_config, &conn));
    error_check(conn->open_session(conn, NULL, NULL, &session));
    error_check(session->create(session, "table:access", table_schema));
    error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));


    // Execute evaluation
    uint8_t key_buf[sizeof(uint64_t) + 1], val_buf[val_len + 1];
    uint8_t* res_buf;
    memset(key_buf, 0, sizeof(uint64_t) + 1);
    memset(val_buf, 0, val_len + 1);

    auto start_time = steady::now();
    // Begin measuring filter size
    std::thread size_measure_thread;
    if (filter) {
        size_measure_thread = std::thread(
            filter_size<zeno_bench::Filter*>, 
            filter, 
            std::ref(file_size)
        );
    }
    
    for (const auto& key : data) {
        const uint64_t key_swapped = __builtin_bswap64(key);
        memcpy(key_buf, &key_swapped, sizeof(key_swapped));
        util::generate_random_string(val_buf, val_len);

        insert_time += util::timing([&] {
            if (filter) {
                res = filter->insert(key);
            }
            insert_kv(cursor, reinterpret_cast<const uint8_t*>(key_buf), val_buf);
        });

        ++inserted;

        // Insertion failed and resizing must be done manually. 
        if ((filter && !res) || (!filter && (inserted == next_sample))) {

            // This if statement is just for filtering out premature filter expansions
            if ((inserted * 2 >= next_sample) && is_period) {
                current_buffer_pool_size_mb =
                        compute_buffer_pool_size_mb(data_size, // next_sample,
                        filter ? (id == 3 ? filter->size() * 1.5 : filter->size()) : 0);
                buffer_string += format_time(start_time,
                                 compute_buffer_pool_size_mb(data_size, 0));
                restart_session(conn, session, cursor, current_buffer_pool_size_mb);

                if (filter == nullptr || next_sample < data_size) {
                    interval = (double)(inserted - prev_interval);
                    prev_interval = inserted;
                    fraction = static_cast<double>(next_sample) / static_cast<double>(data_size);

                    if (record_insert && insert_time != 0) {
                        file_insert << fraction << "," << std::fixed << std::setprecision(12)
                                    << ((double)insert_time) / (interval * 1000.0 /*us*/) << ",";
                        insert_time = 0;
                    }

                    next_sample *= 2;
                }
            }
            if (filter) {
                insert_time += util::timing([&] {
                    filter->resize();
                    filter->insert(key);
                });
                if (fractional) {
                    is_period = !is_period;
                }
            }

        }
    }

    // record the final interval
    if (record_insert && insert_time != 0 && inserted > prev_interval) {
        interval = (double)(inserted - prev_interval);
        file_insert << 1.0 << "," << std::fixed << std::setprecision(12)
                    << ((double)insert_time) / (interval * 1000.0 /*us*/) << ",";
        insert_time = 0;
    }

    stop_measurement.store(true, std::memory_order_relaxed);
    if (size_measure_thread.joinable()) size_measure_thread.join();

    if (!filter) {
        // buffer_string += format_time(start_time, current_buffer_pool_size_mb);
        // file_size << buffer_string;
    }

    if (filter) file_size << std::endl;
    file_pquery << std::endl;
    file_nquery << std::endl;
    file_fpr << std::endl;
    file_insert << std::endl;

    if (filter) {
        // buffer_string += format_time(start_time, current_buffer_pool_size_mb);
        file_size << buffer_string << std::endl;
    }

    file_size.close();
    file_pquery.close();
    file_nquery.close();
    file_fpr.close();
    file_insert.close();

    error_check(cursor->close(cursor));
    error_check(session->close(session, NULL));
    error_check(conn->close(conn, NULL));
    return 0;
}