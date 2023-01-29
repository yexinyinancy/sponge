#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _cur_retransmission_timeout{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _win_start; }

void TCPSender::fill_window() {
    uint64_t need_send_len = 0;
    if (_win_size > (_next_seqno - _win_start)) {
        need_send_len = _win_size - (_next_seqno - _win_start);
    }
    //tesetcase17: _segments_outstanding has syn and _win_size == 0, _win_size should not be one
    if (_win_size == 0 && !_segments_outstanding.empty()) {
        return;
    }
    need_send_len = _win_size == 0 ? 1 : need_send_len;
    while (need_send_len > 0) {
        TCPSegment segment{};
        if (_next_seqno == 0) {
            segment.header().syn = true;
            need_send_len--;
        }
        segment.payload() = Buffer(_stream.read(min(need_send_len, TCPConfig::MAX_PAYLOAD_SIZE)));
        need_send_len -= segment.payload().size();
        //tesetcase16:  _fin_flag: only enter once, otherwise, never break since segment.length_in_sequence_space() != 0 because of fin
        if (_stream.eof() && need_send_len > 0 && !_fin_flag) {
            segment.header().fin = true;
            need_send_len--;
            _fin_flag = true;
        }
        if (segment.length_in_sequence_space() == 0) {
            break;
        }
        segment.header().seqno = wrap(_next_seqno, _isn);
        _segments_out.push(segment);
        _segments_outstanding.push(segment);
        _next_seqno += segment.length_in_sequence_space();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t an = unwrap(ackno, _isn, _win_start);
    //tesetcase15 : when ackno > _next_seqno, just ignored; another method is to process seg in _segments_outstanding, but update the bytes_in_flight
    if (an > _next_seqno) {
        return;
    }
    _win_size = window_size;
    if (an <= _win_start) {
        return;
    }
    _win_start = an;
    while (!_segments_outstanding.empty()) {
        TCPSegment old_seg = _segments_outstanding.front();
        if (unwrap(old_seg.header().seqno, _isn, _next_seqno) + old_seg.length_in_sequence_space() <= an) {
            _segments_outstanding.pop();
        }
        else {
            break;
        }
    }
    _cur_retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_retransmissions = 0;
    _cur_time = 0;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _cur_time += ms_since_last_tick;
    if (_cur_time >= _cur_retransmission_timeout && !_segments_outstanding.empty()) {
        _segments_out.push(_segments_outstanding.front());
        _consecutive_retransmissions++;
        // testcase13: ack_received has never been called (window_size is 0), _cur_retransmission_timeout shoule be doubled
        if (_segments_outstanding.front().header().syn || _win_size != 0) {
            _cur_retransmission_timeout *= 2;
        }
        _cur_time = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}
