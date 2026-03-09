#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

/**
 * @brief The XSKMAP used for redirecting packets to user-space.
 */
struct {
    __uint(type, BPF_MAP_TYPE_XSKMAP);
    __uint(max_entries, 64);
    __type(key, int);
    __type(value, int);
} porth_xsk_map SEC(".maps");

/**
 * @brief Main XDP program logic.
 */
SEC("xdp")
int porth_xdp_prog(struct xdp_md *ctx) {
    // For now, redirect ALL traffic on the interface to queue 0
    // This allows Porth-IO to handle the entire stream.
    return bpf_redirect_map(&porth_xsk_map, ctx->rx_queue_index, XDP_PASS);
}

char _license[] SEC("license") = "GPL";