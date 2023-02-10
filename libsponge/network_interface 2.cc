#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame frame;
    frame.header().src = _ethernet_address;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.payload() = dgram.serialize();
    if (_arp_cache.find(next_hop_ip) != _arp_cache.end() && _arp_cache[next_hop_ip].ttl >= _cur_time) {
        frame.header().dst = _arp_cache[next_hop_ip].address;
        _frames_out.push(frame);
    }
    else {
        _waiting_send_queue.push({next_hop_ip, frame, 0});
        _resend();
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return nullopt;
    }
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        if (datagram.parse(frame.payload()) == ParseResult::NoError) {
            return datagram;
        }
        return nullopt;
    }
    else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage message;
        if (message.parse(frame.payload()) == ParseResult::NoError) {
            if (message.opcode == ARPMessage::OPCODE_REQUEST && message.target_ip_address == _ip_address.ipv4_numeric()) {
                ARPMessage reply;
                reply.opcode = ARPMessage::OPCODE_REPLY;
                reply.sender_ethernet_address = _ethernet_address;
                reply.sender_ip_address = _ip_address.ipv4_numeric();
                reply.target_ethernet_address = message.sender_ethernet_address;
                reply.target_ip_address = message.sender_ip_address;
                EthernetFrame frame_reply;
                frame_reply.header().src = _ethernet_address;
                frame_reply.header().dst = message.sender_ethernet_address;
                frame_reply.header().type = EthernetHeader::TYPE_ARP;
                frame_reply.payload() = reply.serialize();
                _frames_out.push(frame_reply);

                // get sender information
                _arp_cache[message.sender_ip_address] = {message.sender_ethernet_address, _cur_time + 30000};
                while (!_waiting_send_queue.empty()) {
                    if (_arp_cache.find(_waiting_send_queue.front().ip) != _arp_cache.end() && _arp_cache[_waiting_send_queue.front().ip].ttl <= _cur_time) {
                        EthernetFrame frame_send = _waiting_send_queue.front().frame;
                        frame_send.header().dst = _arp_cache[_waiting_send_queue.front().ip].address;
                        _frames_out.push(frame);
                        _waiting_send_queue.pop();
                    }
                    else {
                        break;
                    }
                }
            }
            else if (message.opcode == ARPMessage::OPCODE_REPLY && message.target_ethernet_address == _ethernet_address && message.target_ip_address == _ip_address.ipv4_numeric()) {
                _arp_cache[message.sender_ip_address] = {message.sender_ethernet_address, _cur_time + 30000};
                while (!_waiting_send_queue.empty()) {
                    if (_arp_cache.find(_waiting_send_queue.front().ip) != _arp_cache.end() && _arp_cache[_waiting_send_queue.front().ip].ttl >= _cur_time) {
                        EthernetFrame frame_send = _waiting_send_queue.front().frame;
                        frame_send.header().dst = _arp_cache[_waiting_send_queue.front().ip].address;
                        _frames_out.push(frame_send);
                        _waiting_send_queue.pop();
                    }
                    else {
                        break;
                    }
                }
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _cur_time += ms_since_last_tick;
    // erase timeout mapping
    for (auto itr = _arp_cache.begin(); itr != _arp_cache.end(); ) {
        if (itr->second.ttl < _cur_time) {
            itr = _arp_cache.erase(itr);
        }
        else {
            itr++;
        }
    }
    _resend();
}

void NetworkInterface::_resend() {
    // send arp request again
    if (!_waiting_send_queue.empty()) {
        if (_cur_time >= _waiting_send_queue.front().ttl) {
            ARPMessage msg;
            msg.opcode = ARPMessage::OPCODE_REQUEST;
            msg.sender_ethernet_address = _ethernet_address;
            msg.sender_ip_address = _ip_address.ipv4_numeric();
            msg.target_ethernet_address = {}; // ask for
            msg.target_ip_address = _waiting_send_queue.front().ip; // send previous request that was not send in because of 5s
            EthernetFrame frame_request;
            frame_request.header().src = _ethernet_address;
            frame_request.header().dst = ETHERNET_BROADCAST;
            frame_request.header().type = EthernetHeader::TYPE_ARP;
            frame_request.payload() = msg.serialize();
            _waiting_send_queue.front().ttl = _cur_time + 5000;
            _frames_out.push(frame_request);
        }
    }
}