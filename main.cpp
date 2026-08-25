#include <cassert>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <fstream>
#include <vector>
#include <array>
#include <filesystem>

#include "examples_mrf.h"
#include "mrf.h"
#include "residual_bp_filtered_inserts_only.h"
#include "relaxed_rbp.h"
#include "synch_bp.h"
#include "synch_bp_sequential.h"
#include "relaxed_smart_splash.h"
#include "perf_metrics.h"

using Results = std::vector<std::array<double,2>>;

void print_usage(const char* progname) {
    std::cerr << "Usage:\n"
              << progname << " <BP_algorithm> <mrf> <size|-|default> [<threads>] [<seed>]\n";
}

void print_header(const std::string& algo, const std::string& mrf, uint64_t size, uint64_t threads, uint64_t seed, bool seed_provided) {
    std::cout << "=====================================\n";
    std::cout << "       Algorithm Execution           \n";
    std::cout << "=====================================\n";
    std::cout << std::left << std::setw(12) << "Algorithm:"    << algo << "\n";
    std::cout << std::left << std::setw(12) << "Threads:"      << threads << "\n";
    std::cout << std::left << std::setw(12) << "Seed:"
              << seed << (seed_provided ? " (Using provided seed)" : " (Using default seed)") << "\n";
    std::cout << std::left << std::setw(12) << "MRF Model:"    << mrf << "\n";
    std::cout << std::left << std::setw(12) << "Problem Size:" << size << "\n";
}

void print_results(double accuracy, double max_accuracy = -1.0, bool is_ldpc = false) {
    std::cout << "============= RESULTS =============\n";
    if (is_ldpc) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::left << std::setw(12) << "Accuracy:" << accuracy * 100 << " %\n";
    } else {
        std::cout << std::scientific << std::setprecision(6);
        std::cout << std::left << std::setw(12) << "Accuracy:" << accuracy << "\n";
        if (max_accuracy >= 0)
            std::cout << std::left << std::setw(12) << "Max Accuracy:" << max_accuracy << "\n";
    }
}

void print_perf_metrics(const perf_metrics& metrics) {
    std::cout << "============= Performance Metrics ==========\n";
    std::cout << "Runtime (ms): " << metrics.runtime_ms << std::endl;
    std::cout << "Number of updates: " << metrics.num_updates << std::endl;
}

int main(int argc, const char** argv) {
    if (argc < 4) {
        print_usage(argv[0]);
        return -1;
    }

    std::string BP_algorithm(argv[1]);
    std::string mrfName(argv[2]);
    std::string size_arg(argv[3]);

    uint64_t size;
    if (size_arg == "-" || size_arg == "default") {
        if (mrfName == "ldpc") size = 3 * static_cast<uint64_t>(1e5);
        else if (mrfName == "ising" || mrfName == "potts") size = 1000;
        else size = static_cast<uint64_t>(1e7);
    } else {
        size = std::stoull(size_arg);
    }

    uint64_t threads = 1;
    uint64_t seed = 2;
    bool seed_provided = false;

    if (BP_algorithm == "residual" || BP_algorithm == "synch_bp") {
        if (argc >= 5) {
            seed = std::stoi(argv[4]);
            seed_provided = true;
        }
    } else {
        if (argc >= 5) {
            threads = std::stoull(argv[4]);
        }
        if (argc >= 6) {
            seed = std::stoi(argv[5]);
            seed_provided = true;
        }
    }

    print_header(BP_algorithm, mrfName, size, threads, seed, seed_provided);

    double sensitivity = 1e-5;

    perf_metrics metrics;

    uint64_t queueNum = threads*4;
    uint64_t batchSizePop = 1;
    uint64_t batchSizePush = 1;
    uint64_t splashH = 2;

    if (mrfName == "ldpc") {
        uint64_t n = size, k = 3, l = 6;
        double eps = 0.07;
        MRF_CSR* mrf = examples_mrf::LDPCCodes(n, k, l, eps, seed);
        Results res;

        assert(mrf);

        if (BP_algorithm == "residual") {
            double num_updates, runtime_ms;
            residual_bp_filtered_inserts_only_CSR::solve(mrf, sensitivity, &res, metrics);
        } else if (BP_algorithm == "relaxed-rbp-mq") {
            relaxed_rbp_CSR::solve_mq(mrf, sensitivity, &res, threads, queueNum, batchSizePop, batchSizePush, metrics);
        } else if (BP_algorithm == "relaxed-rbp-smq") {
            relaxed_rbp_CSR::solve_smq(mrf, sensitivity, &res, threads, metrics);
        } else if (BP_algorithm == "synch_bp") {
            synch_bp_sequential_CSR::solve(mrf, sensitivity, &res, metrics);
        } else if (BP_algorithm == "synch_bp_mt") {
            // Uses default 1-hour timeout (3600 seconds)
            synch_bp_CSR::solve(mrf, sensitivity, threads, &res, metrics);
        } else if (BP_algorithm == "relaxed-smart-splash-mq") {
            relaxed_smart_splash_CSR::solve_mq(mrf, sensitivity, &res, threads, queueNum, batchSizePop, batchSizePush, splashH, metrics);
        } else if (BP_algorithm == "relaxed-smart-splash-smq") {
            relaxed_smart_splash_CSR::solve_smq(mrf, sensitivity, &res, threads, splashH, metrics);
        } else {
            std::cerr << "Unrecognized BP_algorithm: " << BP_algorithm << "\n";
            delete mrf;
            return 1;
        }

        int total_errors = 0, corrected = 0;
        for (uint64_t i = 0; i < n; ++i) {
            if (res[i][1] > 0.5) total_errors++;
            if (res[i][0] < 0.5 && res[i][1] > 0.5) corrected++;
        }

        double accuracy = (total_errors > 0 ? double(corrected) / total_errors : 1.0);
        print_results(accuracy, -1.0, true);  // for LDPC
        print_perf_metrics(metrics);
        delete mrf;
    } else {
        Results res;
        MRF* mrf = nullptr;
        if (mrfName == "ising") mrf = examples_mrf::isingMRF(size, 2, seed);
        else if (mrfName == "potts") mrf = examples_mrf::pottsMRF(size, 5, seed);
        else if (mrfName == "tree") mrf = examples_mrf::randomTree(size, 5, seed);
        else if (mrfName == "deterministic_tree") mrf = examples_mrf::deterministicTree(size);
        else {
            std::cerr << "Unrecognized MRF type: " << mrfName << "\n";
            return 1;
        }

        assert(mrf);

        if (BP_algorithm == "residual") {
            residual_bp_filtered_inserts_only::solve(mrf, sensitivity, &res, metrics);
        } else if (BP_algorithm == "relaxed-rbp-mq") {
            relaxed_rbp::solve_mq(mrf, sensitivity, &res, threads, queueNum, batchSizePop, batchSizePush, metrics);
        } else if (BP_algorithm == "relaxed-rbp-smq") {
            relaxed_rbp::solve_smq(mrf, sensitivity, &res, threads, metrics);
        } else if (BP_algorithm == "synch_bp") {
            synch_bp_sequential::solve(mrf, sensitivity, &res, metrics);
        } else if (BP_algorithm == "synch_bp_mt") {
            // Uses default 1-hour timeout (3600 seconds)
            synch_bp::solve(mrf, sensitivity, &res, threads, metrics);
        } else if (BP_algorithm == "relaxed-smart-splash-mq") {
            relaxed_smart_splash::solve_mq(mrf, sensitivity, &res, threads, queueNum, batchSizePop, batchSizePush, splashH, metrics);
        } else if (BP_algorithm == "relaxed-smart-splash-smq") {
            relaxed_smart_splash::solve_smq(mrf, sensitivity, &res, threads, splashH, metrics);
        } else {
            std::cerr << "Unrecognized BP_algorithm: " << BP_algorithm << "\n";
            delete mrf;
            return 1;
        }

        delete mrf;
        uint64_t len = res.size();
        uint64_t wid = res[0].size();

        //WARNING: this requires golden directory to have been created already
        std::string filename = "./golden/golden-" + mrfName + "-" + std::to_string(size) + "-S" + std::to_string(seed);
        
        //if golden directory does not exist, create it
        if (std::filesystem::create_directory("golden")) {
            std::cout << "New golden folder created" << std::endl;
        }
        else {
            std::cout << "Golden folder already existed, not creating a new one" << std::endl;
        }
        
        if (BP_algorithm == "residual") {
            // Skip writing if golden file already exists
            if (std::filesystem::exists(filename)) {
                std::cout << "Golden file already exists, skipping write: " << filename << std::endl;
            }
            else {
                std::ofstream ostrm(filename);
                for (uint64_t i=0; i < len; i++) {
                    for (uint64_t j = 0; j < wid; j++) {
                        ostrm << res[i][j] << " ";
                    }
                    ostrm << "\n";
                }
                std::cout << "Golden file written: " << filename << std::endl;
            }
            print_perf_metrics(metrics); 
        } else {
            //print performance metrics, do not depend on residual results for comparison
            print_perf_metrics(metrics);

            //compute accuracy etc based on residual results, errors out if residual results have not been run to generate golden results folder
            std::ifstream istrm(filename);
            if (!istrm) {
                std::cerr << "Golden file not found: " << filename << "\n";
                return 1;
            }

            Results jury(res.size());
            for (size_t i = 0; i < res.size(); ++i)
                for (size_t j = 0; j < res[i].size(); ++j)
                    istrm >> jury[i][j];

            double totalL1 = 0, maxL1 = 0;
            for (size_t i = 0; i < res.size(); ++i) {
                double L1 = 0;
                for (size_t j = 0; j < res[i].size(); ++j)
                    L1 += std::abs(res[i][j] - jury[i][j]);
                totalL1 += L1;
                maxL1 = std::max(maxL1, L1);
            }
            print_results(totalL1 / res.size(), maxL1, false);
        }
    }

    return 0;
}
