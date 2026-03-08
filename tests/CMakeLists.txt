# Porth-IO: Functional & Physics Tests
# ---------------------------------------------------------

# Helper to avoid repetitive linking logic for standard tests
function(add_porth_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE 
        Porth::porth-core 
        Catch2::Catch2WithMain
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# 1. Logic & Data Plane Tests (Catch2-based)
add_porth_test(unit_ring_buffer unit_ring_buffer.cpp)
add_porth_test(fuzz_ring_buffer fuzz_ring_buffer.cpp)
add_porth_test(test_metrics test_metrics.cpp)

# 2. Physics & Digital Twin Tests (Catch2-based)
add_porth_test(test_sim_phy test_sim_phy.cpp)
add_porth_test(test_sim_protocol test_sim_protocol.cpp)
add_porth_test(test_sim_domains test_sim_domains.cpp)
add_porth_test(test_sim_chaos test_sim_chaos.cpp)
add_porth_test(test_physics_barrier test_physics_barrier.cpp)
add_porth_test(test_physics_response test_physics_response.cpp)