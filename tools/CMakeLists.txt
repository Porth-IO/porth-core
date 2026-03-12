# Porth-IO: Administrative & Telemetry Tools
# ---------------------------------------------------------

add_executable(porth-sim-ctl porth-sim-ctl.cpp)
target_link_libraries(porth-sim-ctl PRIVATE Porth::porth-core)

add_executable(porth_stat porth_stat.cpp)
target_link_libraries(porth_stat PRIVATE Porth::porth-core)

add_executable(porth-chaos porth-chaos.cpp)
target_link_libraries(porth-chaos PRIVATE porth-core)