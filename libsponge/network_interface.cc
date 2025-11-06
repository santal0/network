#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), 
      _ip_address(ip_address),
      _arp_cache(),                  // 初始化ARP缓存
      _pending_datagrams(),          // 初始化等待发送的数据报队列
      _arp_request_cooldown()        // 初始化ARP请求冷却时间
{
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    // 检查ARP缓存中是否有有效的MAC地址
    auto it = _arp_cache.find(next_hop_ip);
    if (it != _arp_cache.end() && it->second.ttl > 0) {
        // 构造IPv4类型的以太网帧
        EthernetFrame frame;
        frame.header().src = _ethernet_address;
        frame.header().dst = it->second.eth_addr;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize().concatenate(); // 序列化IP数据报
        _frames_out.push(frame);
        return;
    }

    // 无有效MAC地址，需发送ARP请求，且最近没有对该 IP 发送过 ARP 查询数据报
    auto cooldown_it = _arp_request_cooldown.find(next_hop_ip);
    if (cooldown_it == _arp_request_cooldown.end() || cooldown_it->second <= 0) {
        // 构造ARP请求
        ARPMessage arp_req;
        arp_req.opcode = ARPMessage::OPCODE_REQUEST;
        arp_req.sender_ethernet_address = _ethernet_address;
        arp_req.sender_ip_address = _ip_address.ipv4_numeric();
        arp_req.target_ip_address = next_hop_ip;
        arp_req.target_ethernet_address = {}; // 目标MAC未知

        // 封装为以太网帧(广播)
        EthernetFrame arp_frame;
        arp_frame.header().src = _ethernet_address;
        arp_frame.header().dst = ETHERNET_BROADCAST; // 广播地址
        arp_frame.header().type = EthernetHeader::TYPE_ARP;
        arp_frame.payload() = arp_req.serialize();
        _frames_out.push(arp_frame);

        // 设置冷却时间(5秒)，避免重复发送
        _arp_request_cooldown[next_hop_ip] = 5000;
    }

    // 将数据报加入等待队列，待ARP应答后发送
    _pending_datagrams[next_hop_ip].push_back(dgram);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // 忽略目标MAC既不是本机也不是广播的数据帧
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return nullopt;
    }

    // 处理IPv4帧
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(frame.payload()) == ParseResult::NoError) {
            return dgram; // 解析成功，返回IP数据报
        }
        return nullopt;
    }

    // 处理ARP帧
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        if (arp.parse(frame.payload()) != ParseResult::NoError || !arp.supported()) {
            return nullopt; // 无效ARP消息
        }

        // 更新ARP缓存的IP-MAC映射(有效期30秒)
        _arp_cache[arp.sender_ip_address] = {arp.sender_ethernet_address, 30000};

        // 若为ARP请求且目标是本机IP，发送ARP应答
        if (arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == _ip_address.ipv4_numeric()) {
            ARPMessage arp_reply;
            arp_reply.opcode = ARPMessage::OPCODE_REPLY;
            arp_reply.sender_ethernet_address = _ethernet_address;
            arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
            arp_reply.target_ethernet_address = arp.sender_ethernet_address;
            arp_reply.target_ip_address = arp.sender_ip_address;

            // 封装为以太网帧，发送
            EthernetFrame reply_frame;
            reply_frame.header().src = _ethernet_address;
            reply_frame.header().dst = arp.sender_ethernet_address;
            reply_frame.header().type = EthernetHeader::TYPE_ARP;
            reply_frame.payload() = arp_reply.serialize();
            _frames_out.push(reply_frame);
        }

        // 发送所有等待该IP的数据报
        auto pending_it = _pending_datagrams.find(arp.sender_ip_address);
        if (pending_it != _pending_datagrams.end()) {
            for (const auto &dgram : pending_it->second) {
                EthernetFrame eth_frame;
                eth_frame.header().src = _ethernet_address;
                eth_frame.header().dst = arp.sender_ethernet_address;
                eth_frame.header().type = EthernetHeader::TYPE_IPv4;
                eth_frame.payload() = dgram.serialize().concatenate();
                _frames_out.push(eth_frame);
            }
            _pending_datagrams.erase(pending_it); // 清空等待队列
        }

        return nullopt;
    }

    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // 更新ARP缓存：移除过期条目
    std::vector<uint32_t> expired_ips;// 过期条目的IP地址
    for (const auto &[ip, entry] : _arp_cache) {
        if (entry.ttl <= ms_since_last_tick) {// 剩余时间太短就是过期了
            expired_ips.push_back(ip);
        } else {// 剩余时间足够，更新TTL
            _arp_cache[ip].ttl -= ms_since_last_tick;
        }
    }
    // 根据过期条目地址，移除过期条目
    for (uint32_t ip : expired_ips) {
        _arp_cache.erase(ip);
    }

    // 更新ARP请求冷却时间
    for (auto &[ip, remaining] : _arp_request_cooldown) {
        if (remaining <= ms_since_last_tick) {
            remaining = 0;
        } else {
            remaining -= ms_since_last_tick;
        }
    }
}