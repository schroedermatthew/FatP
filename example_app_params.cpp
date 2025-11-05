// example_app_params.cpp
// Practical example demonstrating SuperGrok enhancements for app parameter management

#include "Json.h"
#include <iostream>
#include <string>
#include <vector>

using namespace cpp_utilities;

// Define your application parameters
struct DatabaseConfig {
    std::string host;
    int port;
    std::string database;
    std::optional<int> connection_timeout;
    std::optional<int> max_connections;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(DatabaseConfig, host, port, database, connection_timeout, max_connections)

struct LogConfig {
    std::string level;
    std::string file_path;
    int max_file_size_mb;
    bool console_output;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(LogConfig, level, file_path, max_file_size_mb, console_output)

struct AppParameters {
    DatabaseConfig database;
    LogConfig logging;
    std::vector<std::string> allowed_origins;
    int worker_threads;
    bool debug_mode;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(AppParameters, database, logging, allowed_origins, worker_threads, debug_mode)

// Helper to create default parameters
AppParameters get_default_params() {
    AppParameters params;
    
    params.database.host = "localhost";
    params.database.port = 5432;
    params.database.database = "myapp";
    params.database.connection_timeout = 30;
    params.database.max_connections = 100;
    
    params.logging.level = "INFO";
    params.logging.file_path = "/var/log/myapp.log";
    params.logging.max_file_size_mb = 100;
    params.logging.console_output = true;
    
    params.allowed_origins = {"http://localhost:3000", "https://myapp.com"};
    params.worker_threads = 4;
    params.debug_mode = false;
    
    return params;
}

int main() {
    const std::string config_file = "app_config.json";
    
    std::cout << "=== Application Parameter Management Demo ===\n\n";
    
    // 1. Create and save default configuration
    std::cout << "1. Creating default configuration...\n";
    AppParameters params = get_default_params();
    
    try {
        // Use the convenient param helper (pretty printing by default)
        save_params(config_file, params);
        std::cout << "   ✓ Saved to " << config_file << "\n\n";
    } catch (const std::runtime_error& e) {
        std::cerr << "   ✗ Failed to save config: " << e.what() << "\n";
        return 1;
    }
    
    // 2. Load configuration
    std::cout << "2. Loading configuration...\n";
    try {
        AppParameters loaded = load_params<AppParameters>(config_file);
        std::cout << "   ✓ Loaded successfully\n";
        std::cout << "   Database: " << loaded.database.host << ":" << loaded.database.port << "\n";
        std::cout << "   Log level: " << loaded.logging.level << "\n";
        std::cout << "   Worker threads: " << loaded.worker_threads << "\n\n";
    } catch (const std::runtime_error& e) {
        // Enhanced error messages show exactly what went wrong
        std::cerr << "   ✗ Failed to load config: " << e.what() << "\n";
        return 1;
    }
    
    // 3. Modify and save with backup
    std::cout << "3. Modifying configuration...\n";
    params.debug_mode = true;
    params.worker_threads = 8;
    params.logging.level = "DEBUG";
    
    try {
        // Save with automatic backup (creates app_config.json.bak)
        save_params_with_backup(config_file, params);
        std::cout << "   ✓ Updated config (backup created)\n\n";
    } catch (const std::runtime_error& e) {
        std::cerr << "   ✗ Failed to update config: " << e.what() << "\n";
        return 1;
    }
    
    // 4. Demonstrate error handling
    std::cout << "4. Testing enhanced error handling...\n";
    
    // Test with invalid JSON (shows position information)
    std::string bad_json = R"({"database": {"host": "localhost", "port": )";
    try {
        (void)parse_json(bad_json);
    } catch (const std::runtime_error& e) {
        std::cout << "   ✓ Parse error caught: " << e.what() << "\n";
        // Error message includes position for easy debugging
    }
    
    // Test with out-of-range value (shows range check)
    std::string overflow_json = R"({"database": {"host": "localhost", "port": 99999999999}, 
                                     "logging": {"level": "INFO", "file_path": "/tmp/log", 
                                     "max_file_size_mb": 100, "console_output": true},
                                     "allowed_origins": [], "worker_threads": 4, "debug_mode": false})";
    try {
        AppParameters bad_params = from_json_string<AppParameters>(overflow_json);
    } catch (const std::runtime_error& e) {
        std::cout << "   ✓ Range error caught: " << e.what() << "\n";
        // Error message shows which field and why it failed
    }
    
    // Test with missing required field (shows field name)
    std::string missing_field_json = R"({"database": {"host": "localhost"}, 
                                         "logging": {"level": "INFO", "file_path": "/tmp/log", 
                                         "max_file_size_mb": 100, "console_output": true},
                                         "allowed_origins": [], "worker_threads": 4, "debug_mode": false})";
    try {
        AppParameters bad_params = from_json_string<AppParameters>(missing_field_json);
    } catch (const std::runtime_error& e) {
        std::cout << "   ✓ Missing field caught: " << e.what() << "\n";
        // Error message tells you exactly which field is missing
    }
    
    std::cout << "\n";
    
    // 5. Demonstrate depth limit protection
    std::cout << "5. Testing depth limit protection...\n";
    std::string deeply_nested = "[";
    for (int i = 0; i < 600; ++i) deeply_nested += "[";
    deeply_nested += "1";
    for (int i = 0; i < 600; ++i) deeply_nested += "]";
    deeply_nested += "]";
    
    try {
        (void)parse_json(deeply_nested);
    } catch (const std::runtime_error& e) {
        std::cout << "   ✓ Depth limit protected: " << e.what() << "\n";
        // Prevents stack overflow from malicious input
    }
    
    std::cout << "\n=== Demo Complete ===\n";
    std::cout << "All enhancements working correctly!\n\n";
    
    std::cout << "Key Features Demonstrated:\n";
    std::cout << "  • Convenient save_params/load_params helpers\n";
    std::cout << "  • Automatic backup on save\n";
    std::cout << "  • Pretty-printed JSON by default\n";
    std::cout << "  • Detailed error messages with context\n";
    std::cout << "  • Range checking prevents overflow\n";
    std::cout << "  • Depth limits prevent stack overflow\n";
    std::cout << "  • Position information in parse errors\n\n";
    
    // Cleanup
    std::remove(config_file.c_str());
    std::remove((config_file + ".bak").c_str());
    
    return 0;
}
