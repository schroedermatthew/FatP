// test_StateMachine.h
#pragma once

namespace cpp_utilities::testing {
    // Test Suite 1: Construction
    bool test_state_machine_construction();
    bool test_state_machine_default_policy();
    bool test_state_machine_custom_initial_state();
    
    // Test Suite 2: Basic Transitions
    bool test_simple_transition();
    bool test_chained_transitions();
    bool test_self_transition_is_noop();
    bool test_multiple_transitions_same_state();
    
    // Test Suite 3: Strict Policy
    bool test_strict_policy_valid_transitions();
    bool test_strict_policy_invalid_transition_throws();
    bool test_strict_policy_complex_graph();
    bool test_strict_policy_prevents_shortcut();
    
    // Test Suite 4: AnyToAny Policy
    bool test_any_to_any_policy_all_transitions();
    bool test_any_to_any_with_complex_states();
    bool test_any_to_any_with_four_states();
    
    // Test Suite 5: Action Policies
    bool test_noexcept_policy_compiles();
    bool test_throwing_policy_allows_exceptions();
    bool test_throwing_exit_action();
    
    // Test Suite 6: Context Sharing
    bool test_context_shared_between_states();
    bool test_context_persistence_across_transitions();
    bool test_context_modification_visible();
    
    // Test Suite 7: State Queries
    bool test_current_state_index();
    bool test_is_in_state();
    bool test_query_operations_consistency();
    
    // Test Suite 8: Edge Cases
    bool test_single_state_machine();
    bool test_large_state_machine();
    bool test_empty_log_accumulation();
    bool test_initial_state_action_called_once();
    
    // Test Suite 9: Compile-Time Validation
    bool test_compile_time_state_validation();
    bool test_noexcept_specification();
    
    // Test Suite 10: Complex Scenarios
    bool test_workflow_simulation();
    bool test_cyclic_transitions();
    bool test_state_machine_with_custom_initial();
}
