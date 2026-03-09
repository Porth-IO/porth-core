/**
 * @file PorthNetworkPortal.hpp
 * @brief Zero-copy AF_XDP networking portal for kernel bypass.
 *
 * Porth-IO: The Sovereign Logic Layer
 */

#pragma once

#include "porth/PorthRingBuffer.hpp"
#include <string>
#include <stdexcept>
#include <iostream>
#include <format>
#include <utility>
#include <vector>

// AF_XDP and BPF headers for the Linux data path
#include <xdp/xsk.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <net/if.h>
#include <sys/mman.h>

namespace porth {

/**
 * @class PorthNetworkPortal
 * @brief Creates a high-speed "Tunnel" (AF_XDP) for Sovereign data.
 */
class PorthNetworkPortal {
private:
    std::string m_ifname;
    struct xsk_socket* m_xsk{nullptr};
    struct xsk_umem* m_umem{nullptr};
    void* m_umem_area{nullptr};
    struct bpf_object* m_bpf_obj{nullptr};
    
    // UMEM configuration constants
    static constexpr size_t NUM_FRAMES = 4096;
    static constexpr size_t FRAME_SIZE = 2048;
    static constexpr size_t UMEM_SIZE = NUM_FRAMES * FRAME_SIZE;

public:
    /**
     * @brief Constructor: Prepares the tunnel for a specific interface.
     */
    explicit PorthNetworkPortal(const std::string& interface_name) 
        : m_ifname(interface_name) {
        
        if (m_ifname.empty()) {
            throw std::runtime_error("Porth-Portal: Interface name cannot be empty.");
        }

        // Allocate the UMEM area (The shared memory "Pool")
        m_umem_area = mmap(NULL, UMEM_SIZE, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        if (m_umem_area == MAP_FAILED) {
            throw std::runtime_error("Porth-Portal: Failed to allocate UMEM area.");
        }

        std::cout << std::format("[Porth-Portal] AF_XDP Tunnel prepared for: {}\n", m_ifname);
        std::cout << std::format("[Porth-Portal] UMEM Pool allocated at: {}\n", m_umem_area);
    }

    /**
     * @brief load_kernel_program: Loads the eBPF bytecode into the kernel.
     * @param path Path to the porth_xdp_kern.o file.
     */
    void load_kernel_program(const std::string& path) {
        m_bpf_obj = bpf_object__open_file(path.c_str(), NULL);
        if (!m_bpf_obj) {
            throw std::runtime_error("Porth-Portal: Failed to open BPF object file.");
        }

        if (bpf_object__load(m_bpf_obj)) {
            throw std::runtime_error("Porth-Portal: Failed to load BPF object into kernel.");
        }

        std::cout << "[Porth-Portal] Sovereign eBPF program loaded into kernel.\n";
    }

    /**
     * @brief Destructor: Ensures resources are safely released.
     */
    ~PorthNetworkPortal() {
        if (m_bpf_obj) {
            bpf_object__close(m_bpf_obj);
        }
        if (m_umem_area) {
            munmap(m_umem_area, UMEM_SIZE);
        }
    }

    // Rule of Five (Deleted copies, Move-only)
    PorthNetworkPortal(const PorthNetworkPortal&) = delete;
    auto operator=(const PorthNetworkPortal&) -> PorthNetworkPortal& = delete;

    PorthNetworkPortal(PorthNetworkPortal&& other) noexcept 
        : m_ifname(std::move(other.m_ifname)), 
          m_xsk(other.m_xsk), 
          m_umem(other.m_umem),
          m_umem_area(other.m_umem_area),
          m_bpf_obj(other.m_bpf_obj) {
        other.m_xsk = nullptr;
        other.m_umem = nullptr;
        other.m_umem_area = nullptr;
        other.m_bpf_obj = nullptr;
    }

    auto operator=(PorthNetworkPortal&& other) noexcept -> PorthNetworkPortal& {
        if (this != &other) {
            m_ifname = std::move(other.m_ifname);
            m_xsk = other.m_xsk;
            m_umem = other.m_umem;
            m_umem_area = other.m_umem_area;
            m_bpf_obj = other.m_bpf_obj;
            other.m_xsk = nullptr;
            other.m_umem = nullptr;
            other.m_umem_area = nullptr;
            other.m_bpf_obj = nullptr;
        }
        return *this;
    }

    [[nodiscard]] auto is_active() const noexcept -> bool {
        return !m_ifname.empty();
    }

    [[nodiscard]] auto get_interface() const noexcept -> std::string {
        return m_ifname;
    }
};

} // namespace porth