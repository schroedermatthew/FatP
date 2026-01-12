/**
 * @file test_MemoryMappedFile.cpp
 * @brief Comprehensive unit tests for MemoryMappedFile.h
 */
/*
FATP_META:
  meta_version: 1
  component: MemoryMappedFile
  file_role: test
  path: tests/test_MemoryMappedFile.cpp
  namespace: fat_p::testing::memorymappedfile
  summary: "Unit tests for MemoryMappedFile."
  related:
    docs_search: "MemoryMappedFile"
    headers:
      - fat_p/MemoryMappedFile.h
      - fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 1
    defines_unprefixed: 1
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdio>  // For std::remove

#include "MemoryMappedFile.h"
#include "FatPTest.h"
#include "test_FatP.h"

namespace fat_p::testing::memorymappedfile
{

using fat_p::testing::artifact_file;

// Helper: Create test file
void create_test_file(const std::string& filename, const std::string& content) {
    std::ofstream file(filename, std::ios::binary);
    file << content;
}

// Helper: Remove test file (C++17 compatible)
void remove_test_file(const std::string& filename) {
    std::remove(filename.c_str());
}

FATP_TEST_CASE(memory_mapped_file_read_only_mapping) {
    const std::string filename = artifact_file("test_mmap_ro.txt");
    const std::string content = "Hello, Memory Mapped File!";
    
    create_test_file(filename, content);
    
    MemoryMappedFile file(filename, MemoryMappedFile::Mode::ReadOnly);
    
    FATP_ASSERT_TRUE(file.is_open(), "File should be open");
    FATP_ASSERT_TRUE(file.size() == content.size(), "Size should match");
    
    auto span = file.get_span<const char>();
    std::string result(span.data(), span.size());
    FATP_ASSERT_TRUE(result == content, "Content should match");
    
    remove_test_file(filename);
    return true;
}

FATP_TEST_CASE(memory_mapped_file_read_write_mapping) {
    const std::string filename = artifact_file("test_mmap_rw.txt");
    const std::string content = "Original Content";
    
    create_test_file(filename, content);
    
    {
        MemoryMappedFile file(filename, MemoryMappedFile::Mode::ReadWrite);
        
        FATP_ASSERT_TRUE(file.is_open(), "File should be open");
        
        auto span = file.get_span<char>();
        span[0] = 'M';  // Modify first character
        span[1] = 'o';
        
        file.flush(false);  // Sync to disk
    }
    
    // Verify changes persisted
    {
        MemoryMappedFile file(filename, MemoryMappedFile::Mode::ReadOnly);
        auto span = file.get_span<const char>();
        FATP_ASSERT_TRUE(span[0] == 'M', "Changes should persist");
        FATP_ASSERT_TRUE(span[1] == 'o', "Changes should persist");
    }
    
    remove_test_file(filename);
    return true;
}

FATP_TEST_CASE(memory_mapped_file_large_file) {
    const std::string filename = artifact_file("test_mmap_large.bin");
    constexpr size_t SIZE = 10 * 1024 * 1024;  // 10MB
    
    // Create large file
    {
        std::ofstream file(filename, std::ios::binary);
        std::vector<char> data(SIZE, 'X');
        file.write(data.data(), SIZE);
    }
    
    MemoryMappedFile file(filename);
    
    FATP_ASSERT_TRUE(file.is_open(), "File should be open");
    FATP_ASSERT_TRUE(file.size() == SIZE, "Size should match");
    
    auto span = file.get_span<const char>();
    FATP_ASSERT_TRUE(span.size() == SIZE, "Span size should match");
    FATP_ASSERT_TRUE(span[0] == 'X', "Content should be accessible");
    FATP_ASSERT_TRUE(span[SIZE-1] == 'X', "End should be accessible");
    
    remove_test_file(filename);
    return true;
}

FATP_TEST_CASE(memory_mapped_file_move_semantics) {
    const std::string filename = artifact_file("test_mmap_move.txt");
    create_test_file(filename, "Move Test");
    
    MemoryMappedFile file1(filename);
    FATP_ASSERT_TRUE(file1.is_open(), "file1 should be open");
    
    MemoryMappedFile file2 = std::move(file1);
    FATP_ASSERT_TRUE(!file1.is_open(), "file1 should be closed after move");
    FATP_ASSERT_TRUE(file2.is_open(), "file2 should be open after move");
    
    remove_test_file(filename);
    return true;
}

FATP_TEST_CASE(memory_mapped_file_empty_file) {
    const std::string filename = artifact_file("test_mmap_empty.txt");
    create_test_file(filename, "");
    
    MemoryMappedFile file(filename);
    
    FATP_ASSERT_TRUE(file.is_open(), "Empty file should be open");
    FATP_ASSERT_TRUE(file.size() == 0, "Size should be zero");
    
    auto span = file.get_span<const char>();
    FATP_ASSERT_TRUE(span.size() == 0, "Span should be empty");
    FATP_ASSERT_TRUE(span.empty(), "Span should report empty");
    
    remove_test_file(filename);
    return true;
}

FATP_TEST_CASE(memory_mapped_file_span_operations) {
    const std::string filename = artifact_file("test_mmap_span.txt");
    const std::string content = "0123456789";
    
    create_test_file(filename, content);
    
    MemoryMappedFile file(filename);
    auto span = file.get_span<const char>();
    
    FATP_ASSERT_TRUE(span.size() == 10, "Span size should be 10");
    FATP_ASSERT_TRUE(!span.empty(), "Span should not be empty");
    FATP_ASSERT_TRUE(span.front() == '0', "Front should be '0'");
    FATP_ASSERT_TRUE(span.back() == '9', "Back should be '9'");
    FATP_ASSERT_TRUE(span[5] == '5', "Index access should work");
    
    // Test subviews
    auto first = span.first(3);
    FATP_ASSERT_TRUE(first.size() == 3, "First 3 elements");
    FATP_ASSERT_TRUE(first[0] == '0' && first[1] == '1' && first[2] == '2', "First subspan content");
    
    auto last = span.last(3);
    FATP_ASSERT_TRUE(last.size() == 3, "Last 3 elements");
    FATP_ASSERT_TRUE(last[0] == '7' && last[1] == '8' && last[2] == '9', "Last subspan content");
    
    auto sub = span.subspan(3, 4);
    FATP_ASSERT_TRUE(sub.size() == 4, "Subspan of 4 elements");
    FATP_ASSERT_TRUE(sub[0] == '3' && sub[3] == '6', "Subspan content");
    
    // Test iterators
    int count = 0;
    for (auto c : span) {
        FATP_ASSERT_TRUE(c == '0' + count, "Iterator should match content");
        ++count;
    }
    FATP_ASSERT_TRUE(count == 10, "Iterator should traverse all elements");
    
    remove_test_file(filename);
    return true;
}

void benchmark_memory_mapped_file() {
    std::cout << "\n" << colors::cyan() << "MemoryMappedFile Benchmarks:" << colors::reset() << "\n\n";
    
    const std::string filename = artifact_file("benchmark_mmap.bin");
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

} // namespace fat_p::testing::memorymappedfile

namespace fat_p::testing
{

bool test_MemoryMappedFile() {

    FATP_PRINT_HEADER(MEMORY MAPPED FILE)

    TestRunner runner;
    
    FATP_RUN_TEST_NS(runner, memorymappedfile, memory_mapped_file_read_only_mapping);
    FATP_RUN_TEST_NS(runner, memorymappedfile, memory_mapped_file_read_write_mapping);
    FATP_RUN_TEST_NS(runner, memorymappedfile, memory_mapped_file_large_file);
    FATP_RUN_TEST_NS(runner, memorymappedfile, memory_mapped_file_move_semantics);
    FATP_RUN_TEST_NS(runner, memorymappedfile, memory_mapped_file_empty_file);
    FATP_RUN_TEST_NS(runner, memorymappedfile, memory_mapped_file_span_operations);

    memorymappedfile::benchmark_memory_mapped_file();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_MemoryMappedFile() ? 0 : 1;
}
#endif
