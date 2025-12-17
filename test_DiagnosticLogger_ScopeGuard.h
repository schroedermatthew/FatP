/**
 * @file test_DiagnosticLogger_ScopeGuard.h
 * @brief Header for DiagnosticLogger ScopeGuard integration tests
 */
#pragma once

namespace fat_p::testing
{

/**
 * @brief Run all ScopeGuard integration tests for DiagnosticLogger
 * 
 * Tests include:
 * - RotatingFileSink guaranteed file reopening with ScopeGuard
 * - ResilientSink automatic failure state management
 * - Test utilities for temporary state changes
 * - Integration tests
 * 
 * @return true if all tests passed, false otherwise
 */
bool test_DiagnosticLogger_ScopeGuard();

} // namespace fat_p::testing
