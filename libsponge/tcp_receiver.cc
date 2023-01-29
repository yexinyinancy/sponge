#include "tcp_receiver.hh"
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    WrappingInt32 seqno = header.seqno;
    string data = seg.payload().copy();
    if (_SYN_flag && header.syn) {
        return;
    }
    if (!_SYN_flag && !header.syn) {
        return;
    }
    if (header.fin) {
        if (_FIN_flag) {
            return;
        }
        _FIN_flag = true;
    }
    if (header.syn) {
        _SYN_flag = true;
        _isn = header.seqno;
        seqno = seqno + 1;
    }
    uint64_t checkpoint = _reassembler.stream_out().bytes_written();
    uint64_t seqno64 = unwrap(seqno, _isn, checkpoint);
    if (seqno64 == 0) {
        return;
    }
    _reassembler.push_substring(data, seqno64 - 1, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_SYN_flag) {
        return nullopt;
    }
    uint64_t ackno = _reassembler.stream_out().bytes_written() + 1;
    if (_reassembler.stream_out().input_ended()) {
        ackno = ackno + 1;
    }
    return wrap(ackno, _isn);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
