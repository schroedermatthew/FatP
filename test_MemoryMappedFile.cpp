#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdio>  // For std::remove

#include "MemoryMappedFile.h"
#include "test_MemoryMappedFile.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

// Helper: Create test file
void create_test_file(const std::string& filename, const std::string& content) {
    std::ofstream file(filename, std::ios::binary);
    file << content;
}

// Helper: Remove test file (C++17 compatible)
void remove_test_file(const std::string& filename) {
    std::remove(filename.c_str());
}

bool test_memory_mapped_file_read_only_mapping() {
    const std::string filename = "test_mmap_ro.txt";
    const std::string content = "Hello, Memory Mapped File!";
    
    create_test_file(filename, content);
    
    MemoryMappedFile file(filename, MemoryMappedFile::Mode::ReadOnly);
    
    SIMPLE_ASSERT(file.is_open(), "File should be open");
    SIMPLE_ASSERT(file.size() == content.size(), "Size should match");
    
    auto span = file.get_span<const char>();
    std::string result(span.data(), span.size());
    SIMPLE_ASSERT(result == content, "Content should match");
    
    remove_test_file(filename);
    return true;
}

bool test_memory_mapped_file_read_write_mapping() {
    const std::string filename = "test_mmap_rw.txt";
    const std::string content = "Original Content";
    
    create_test_file(filename, content);
    
    {
        MemoryMappedFile file(filename, MemoryMappedFile::Mode::ReadWrite);
        
        SIMPLE_ASSERT(file.is_open(), "File should be open");
        
        auto span = file.get_span<char>();
        span[0] = 'M';  // Modify first character
        span[1] = 'o';
        
        file.flush(false);  // Sync to disk
    }
    
    // Verify changes persisted
    {
        MemoryMappedFile file(filename, MemoryMappedFile::Mode::ReadOnly);
        auto span = file.get_span<const char>();
        SIMPLE_ASSERT(span[0] == 'M', "Changes should persist");
        SIMPLE_ASSERT(span[1] == 'o', "Changes should persist");
    }
    
    remove_test_file(filename);
    return true;
}

bool test_memory_mapped_file_large_file() {
    const std::string filename = "test_mmap_large.bin";
    constexpr size_t SIZE = 10 * 1024 * 1024;  // 10MB
    
    // Create large file
    {
        std::ofstream file(filename, std::ios::binary);
        std::vector<char> data(SIZE, 'X');
        file.write(data.data(), SIZE);
    }
    
    MemoryMappedFile file(filename);
    
    SIMPLE_ASSERT(file.is_open(), "File should be open");
    SIMPLE_ASSERT(file.size() == SIZE, "Size should match");
    
    auto span = file.get_span<const char>();
    SIMPLE_ASSERT(span.size() == SIZE, "Span size should match");
    SIMPLE_ASSERT(span[0] == 'X', "Content should be accessible");
    SIMPLE_ASSERT(span[SIZE-1] == 'X', "End should be accessible");
    
    remove_test_file(filename);
    return true;
}

bool test_memory_mapped_file_move_semantics() {
    const std::string filename = "test_mmap_move.txt";
    create_test_file(filename, "Move Test");
    
    MemoryMappedFile file1(filename);
    SIMPLE_ASSERT(file1.is_open(), "file1 should be open");
    
    MemoryMappedFile file2 = std::move(file1);
    SIMPLE_ASSERT(!file1.is_open(), "file1 should be closed after move");
    SIMPLE_ASSERT(file2.is_open(), "file2 should be open after move");
    
    remove_test_file(filename);
    return true;
}

bool test_memory_mapped_file_empty_file() {
    const std::string filename = "test_mmap_empty.txt";
    create_test_file(filename, "");
    
    MemoryMappedFile file(filename);
    
    SIMPLE_ASSERT(file.is_open(), "Empty file should be open");
    SIMPLE_ASSERT(file.size() == 0, "Size should be zero");
    
    auto span = file.get_span<const char>();
    SIMPLE_ASSERT(span.size() == 0, "Span should be empty");
    SIMPLE_ASSERT(span.empty(), "Span should report empty");
    
    remove_test_file(filename);
    return true;
}

bool test_memory_mapped_file_span_operations() {
    const std::string filename = "test_mmap_span.txt";
    const std::string content = "0123456789";
    
    create_test_file(filename, content);
    
    MemoryMappedFile file(filename);
    auto span = file.get_span<const char>();
    
    SIMPLE_ASSERT(span.size() == 10, "Span size should be 10");
    SIMPLE_ASSERT(!span.empty(), "Span should not be empty");
    SIMPLE_ASSERT(span.front() == '0', "Front should be '0'");
    SIMPLE_ASSERT(span.back() == '9', "Back should be '9'");
    SIMPLE_ASSERT(span[5] == '5', "Index access should work");
    
    // Test subviews
    auto first = span.first(3);
    SIMPLE_ASSERT(first.size() == 3, "First 3 elements");
    SIMPLE_ASSERT(first[0] == '0' && first[1] == '1' && first[2] == '2', "First subspan content");
    
    auto last = span.last(3);
    SIMPLE_ASSERT(last.size() == 3, "Last 3 elements");
    SIMPLE_ASSERT(last[0] == '7' && last[1] == '8' && last[2] == '9', "Last subspan content");
    
    auto sub = span.subspan(3, 4);
    SIMPLE_ASSERT(sub.size() == 4, "Subspan of 4 elements");
    SIMPLE_ASSERT(sub[0] == '3' && sub[3] == '6', "Subspan content");
    
    // Test iterators
    int count = 0;
    for (auto c : span) {
        SIMPLE_ASSERT(c == '0' + count, "Iterator should match content");
        ++count;
    }
    SIMPLE_ASSERT(count == 10, "Iterator should traverse all elements");
    
    remove_test_file(filename);
    return true;
}

void benchmark_memory_mapped_file() {
    std::cout << "\n" << colors::cyan() << "MemoryMappedFile Benchmarks:" << colors::reset() << "\n\n";
    
    const std::string filename = "benchmark_mmap.bin";
    constexpr size_t SIZE = 1024 * 1024;  // 1MB
    
    // Create file
    {
        std::ofstream file(filename, std::ios::binary);
        std::vector<char> data(SIZE, 'X');
        file.write(data.data(), SIZE);
    }
    
    // Benchmark: Memory-mapped sequential read
    {
        MemoryMappedFile file(filename);
        uint64_t sum = 0;
        
        double time = measure_perf([&]() {
            auto span = file.get_span<const uint8_t>();
            for (size_t i = 0; i < span.size(); i += 64) {
                sum += span[i];
            }
            DoNotOptimize(sum);
        }, 100, 10);
        
        std::cout << "Sequential read (mmap): " << format_time(time) << "\n";
    }
    
    // Benchmark: Traditional fread
    {
        std::vector<char> buffer(SIZE);
        
        double time = measure_perf([&]() {
            FILE* f = fopen(filename.c_str(), "rb");
            fread(buffer.data(), 1, SIZE, f);
            fclose(f);
            uint64_t sum = 0;
            for (size_t i = 0; i < SIZE; i += 64) {
                sum += buffer[i];
            }
            DoNotOptimize(sum);
        }, 100, 10);
        
        std::cout << "Sequential read (fread): " << format_time(time) << "\n";
    }
    
    // Benchmark: Random access with mmap
    {
        MemoryMappedFile file(filename);
        uint64_t sum = 0;
        
        double time = measure_perf([&]() {
            auto span = file.get_span<const uint8_t>();
            // Simulate random access pattern
            for (size_t i = 0; i < 1000; ++i) {
                size_t idx = (i * 997) % span.size();  // Pseudo-random
                sum += span[idx];
            }
            DoNotOptimize(sum);
        }, 1000, 10);
        
        std::cout << "Random access (mmap): " << format_time(time) << "\n";
    }
    
    // Benchmark: Random access with fseek+fread
    {
        uint64_t sum = 0;
        char byte;
        
        double time = measure_perf([&]() {
            FILE* f = fopen(filename.c_str(), "rb");
            for (size_t i = 0; i < 1000; ++i) {
                size_t idx = (i * 997) % SIZE;
                fseek(f, static_cast<long>(idx), SEEK_SET);
                fread(&byte, 1, 1, f);
                sum += byte;
            }
            fclose(f);
            DoNotOptimize(sum);
        }, 1000, 10);
        
        std::cout << "Random access (fseek): " << format_time(time) << "\n";
    }
    
    remove_test_file(filename);
}

bool test_MemoryMappedFile() {

    PRINT_HEADER(MEMORY MAPPED FILE)

    TestRunner runner;
    
    RUN_TEST(runner, memory_mapped_file_read_only_mapping);
    RUN_TEST(runner, memory_mapped_file_read_write_mapping);
    RUN_TEST(runner, memory_mapped_file_large_file);
    RUN_TEST(runner, memory_mapped_file_move_semantics);
    RUN_TEST(runner, memory_mapped_file_empty_file);
    RUN_TEST(runner, memory_mapped_file_span_operations);

    benchmark_memory_mapped_file();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
