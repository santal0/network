#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // Your code here.
        // 将路由条目添加到路由表
    _routing_table.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // 1. 处理TTL：若TTL≤1，丢弃数据报
    if (dgram.header().ttl <= 1) {
        return;
    }
    // TTL减1，并重置校验和（因为TTL变化会导致校验和无效）
    dgram.header().ttl -= 1;
    dgram.header().cksum = 0;  // 后续序列化时会重新计算校验和

    // 2. 提取目的IP地址（32位数值）
    const uint32_t dst_ip = dgram.header().dst;
    optional<RouteEntry> best_route;  // 用于存储最佳匹配路由

    // 3. 最长前缀匹配：遍历路由表寻找最佳路由
    for (const auto &entry : _routing_table) {
        // 计算前缀掩码（如前缀长度24，掩码为0xFFFFFF00）
        const uint32_t mask = (entry.prefix_length == 0) 
            ? 0 
            : 0xFFFFFFFF << (32 - entry.prefix_length);

        // 提取路由前缀和目的IP的前缀（与掩码做与运算）
        const uint32_t route_prefix_masked = entry.route_prefix & mask;
        const uint32_t dst_prefix_masked = dst_ip & mask;

        // 若前缀匹配，且当前路由前缀更长，则更新最佳路由
        if (route_prefix_masked == dst_prefix_masked) {
            if (!best_route.has_value() || entry.prefix_length > best_route->prefix_length) {
                best_route = entry;
            }
        }
    }

    // 4. 若没有匹配的路由，丢弃数据报
    if (!best_route.has_value()) {
        return;
    }

    // 5. 确定下一跳地址：若路由有下一跳则使用，否则直接发送到目的IP
    const Address next_hop = best_route->next_hop.has_value()
        ? best_route->next_hop.value()
        : Address::from_ipv4_numeric(dst_ip);

    // 6. 通过指定接口发送数据报
    _interfaces[best_route->interface_num].send_datagram(dgram, next_hop);
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}