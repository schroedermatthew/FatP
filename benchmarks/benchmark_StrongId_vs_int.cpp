/**
 * @file benchmark_StrongId_vs_int.cpp
 * @brief Comparative performance benchmark: StrongId vs raw int
 * 
 * This benchmark validates the "zero overhead abstraction" claim by comparing
 * StrongId operations directly against equivalent raw int operations.
 * 
 * Compile with:
 *   g++ -std=c++17 -O3 -DNDEBUG -I/mnt/project benchmark_StrongId_vs_int.cpp -o benchmark_strongid -lpthread
 * 
 * For fair comparison, both are compiled with full optimization (-O3).
 */

#include "StrongId.h"
#include "FatPTest.h"
#include <unordered_set>
#include <vector>

namespace fat_p::testing {

// Define test types
struct BenchmarkIdTag {};
using BenchmarkId = StrongId<int, BenchmarkIdTag>;
using AtomicBenchmarkId = AtomicStrongId<int, BenchmarkIdTag>;

// =============================================================================
// Comparative Benchmarks: StrongId vs Raw int
// =============================================================================

void run_comparative_benchmarks() {
    auto& out = *get_test_config().output;
    
    out << "\n" << colors::cyan() << colors::bold() 
        << "========================================================\n"
        << "  StrongId vs Raw int - Comparative Performance\n"
        << "========================================================" 
        << colors::reset() << "\n\n";

    out << colors::yellow() 
        << "Note: Near-identical times validate zero-overhead abstraction.\n"
        << "Timer resolution warnings indicate operations are extremely fast.\n"
        << colors::reset() << "\n\n";

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------
    out << colors::blue() << "--- Construction ---" << colors::reset() << "\n";
    
    benchmark_compare(
        "StrongId construction",
        []() { volatile BenchmarkId id(42); (void)id; },
        "Raw int construction", 
        []() { volatile int id = 42; (void)id; }
    );
    out << "\n";

    // -------------------------------------------------------------------------
    // Value Access
    // -------------------------------------------------------------------------
    out << colors::blue() << "--- Value Access ---" << colors::reset() << "\n";
    
    BenchmarkId sid(42);
    int raw_id = 42;
    
    benchmark_compare(
        "StrongId::get()",
        [&sid]() { volatile int x = sid.get(); (void)x; },
        "Raw int read",
        [&raw_id]() { volatile int x = raw_id; (void)x; }
    );
    out << "\n";

    // -------------------------------------------------------------------------
    // Comparison Operations
    // -------------------------------------------------------------------------
    out << colors::blue() << "--- Comparison ---" << colors::reset() << "\n";
    
    BenchmarkId sid1(42), sid2(100);
    int raw1 = 42, raw2 = 100;
    
    benchmark_compare(
        "StrongId operator<",
        [&sid1, &sid2]() { volatile bool r = sid1 < sid2; (void)r; },
        "Raw int operator<",
        [&raw1, &raw2]() { volatile bool r = raw1 < raw2; (void)r; }
    );
    out << "\n";
    
    benchmark_compare(
        "StrongId operator==",
        [&sid1, &sid2]() { volatile bool r = sid1 == sid2; (void)r; },
        "Raw int operator==",
        [&raw1, &raw2]() { volatile bool r = raw1 == raw2; (void)r; }
    );
    out << "\n";

    // -------------------------------------------------------------------------
    // Arithmetic Operations (with checked arithmetic overhead)
    // -------------------------------------------------------------------------
    out << colors::blue() << "--- Arithmetic (checked vs unchecked) ---" << colors::reset() << "\n";
    
    benchmark_compare(
        "StrongId addition (checked)",
        [&sid1]() { volatile BenchmarkId r = sid1 + 10; (void)r; },
        "Raw int addition (unchecked)",
        [&raw1]() { volatile int r = raw1 + 10; (void)r; }
    );
    out << "\n";
    
    benchmark_compare(
        "StrongId multiplication (checked)",
        [&sid1]() { volatile BenchmarkId r = sid1 * 2; (void)r; },
        "Raw int multiplication (unchecked)",
        [&raw1]() { volatile int r = raw1 * 2; (void)r; }
    );
    out << "\n";
    
    benchmark_compare(
        "StrongId increment (checked)",
        [&sid1]() { 
            BenchmarkId temp = sid1;
            ++temp;
            volatile int x = temp.get(); (void)x;
        },
        "Raw int increment (unchecked)",
        [&raw1]() { 
            int temp = raw1;
            ++temp;
            volatile int x = temp; (void)x;
        }
    );
    out << "\n";

    // -------------------------------------------------------------------------
    // Hash Operations
    // -------------------------------------------------------------------------
    out << colors::blue() << "--- Hashing ---" << colors::reset() << "\n";
    
    std::hash<BenchmarkId> sid_hasher;
    std::hash<int> int_hasher;
    
    benchmark_compare(
        "StrongId hash",
        [&sid1, &sid_hasher]() { volatile size_t h = sid_hasher(sid1); (void)h; },
        "Raw int hash",
        [&raw1, &int_hasher]() { volatile size_t h = int_hasher(raw1); (void)h; }
    );
    out << "\n";

    // -------------------------------------------------------------------------
    // Container Operations
    // -------------------------------------------------------------------------
    out << colors::blue() << "--- Container Operations ---" << colors::reset() << "\n";
    
    std::unordered_set<BenchmarkId> sid_set;
    std::unordered_set<int> int_set;
    
    // Pre-populate sets
    for (int i = 0; i < 1000; ++i) {
        sid_set.insert(BenchmarkId(i));
        int_set.insert(i);
    }
    
    BenchmarkId lookup_sid(500);
    int lookup_int = 500;
    
    benchmark_compare(
        "StrongId set lookup",
        [&sid_set, &lookup_sid]() { 
            volatile bool found = sid_set.count(lookup_sid) > 0; 
            (void)found; 
        },
        "Raw int set lookup",
        [&int_set, &lookup_int]() { 
            volatile bool found = int_set.count(lookup_int) > 0; 
            (void)found; 
        }
    );
    out << "\n";

    // -------------------------------------------------------------------------
    // Atomic Operations
    // -------------------------------------------------------------------------
    out << colors::blue() << "--- Atomic Operations ---" << colors::reset() << "\n";
    
    AtomicBenchmarkId atomic_sid(BenchmarkId(42));
    std::atomic<int> atomic_int(42);
    
    benchmark_compare(
        "AtomicStrongId load",
        [&atomic_sid]() { volatile BenchmarkId x = atomic_sid.load(); (void)x; },
        "atomic<int> load",
        [&atomic_int]() { volatile int x = atomic_int.load(); (void)x; }
    );
    out << "\n";
    
    benchmark_compare(
        "AtomicStrongId store",
        [&atomic_sid]() { atomic_sid.store(BenchmarkId(42)); },
        "atomic<int> store",
        [&atomic_int]() { atomic_int.store(42); }
    );
    out << "\n";

    // -------------------------------------------------------------------------
    // Summary Table
    // -------------------------------------------------------------------------
    out << colors::cyan() << colors::bold() 
        << "\n========================================================\n"
        << "  Summary\n"
        << "========================================================" 
        << colors::reset() << "\n\n";
    
    out << "Operations where StrongId and raw int show identical timing\n"
        << "confirm that the type wrapper compiles away completely.\n\n"
        << "Arithmetic operations may show slight overhead due to\n"
        << "checked arithmetic (overflow detection) - this is a\n"
        << colors::green() << "safety feature" << colors::reset() 
        << ", not wrapper overhead.\n\n";
    
    out << "To disable checked arithmetic and match raw int speed,\n"
        << "provide a custom OpPolicy with unchecked operations.\n";
}

} // namespace fat_p::testing

int main() {
    fat_p::testing::run_comparative_benchmarks();
    return 0;
}
