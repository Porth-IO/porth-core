# Porth-IO: Administrative & Telemetry Tools
# ---------------------------------------------------------

add_executable(porth-sim-ctl porth-sim-ctl.cpp)
target_link_libraries(porth-sim-ctl PRIVATE Porth::porth-core)