#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _cur_time; }

void TCPConnection::_send_segment() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win = _receiver.window_size();
        _segments_out.push(seg);
    }
}

void TCPConnection::_send_rst_segment() {
    std::queue<TCPSegment> seg_out = _sender.segments_out();
    if (seg_out.empty()) {
        _sender.send_empty_segment();
    }
    seg_out = _sender.segments_out();
    TCPSegment seg = seg_out.front();
    seg_out.pop();
    if (_receiver.ackno().has_value()) {
        seg.header().ack = true;
        seg.header().ackno = _receiver.ackno().value();
    }
    seg.header().win = _receiver.window_size();
    seg.header().rst = true;
    _segments_out.push(seg);
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _cur_time = 0;
    // if the rst (reset) flag is set, sets both the inbound and outbound streams to the error state and kills the connection permanently
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active_flag = false;
        return;
    }
    // gives the segment to the TCPReceiver so it can inspect the fields it cares about on incoming segments: seqno, syn , payload, and fin
    _receiver.segment_received(seg);

    // if the ack flag is set, tells the TCPSender about the fields it cares about on incoming segments: ackno and window size
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _send_segment();
    }
    // if the incoming segment occupied any sequence numbers, the TCPConnection makes sure that at least one segment is sent in reply, to reflect an update in the ackno and window size
    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
            _send_segment();
        }
        else {
            _send_segment();
        }
    }

    if (_receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }
    
    // There is one extra special case that you will have to handle in the TCPConnection’s segment received() method
    if (_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0 && seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        _send_segment();
    }
}

bool TCPConnection::active() const { return _active_flag; }

size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    _send_segment();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _cur_time += ms_since_last_tick;
    // tell the TCPSender about the passage of time
    _sender.tick(ms_since_last_tick);
    // abort the connection, and send a reset segment to the peer (an empty segment with the rst flag set), if the number of consecutive retransmissions is more than an upper limit TCPConfig::MAX RETX ATTEMPTS
    if (_sender.segments_out().size() > 0) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win = _receiver.window_size();
        // abort the connection
        if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            seg.header().rst = true;
            _active_flag = false;
        }
        _segments_out.push(seg);
    }
    
    // end the connection cleanly if necessary
    bool prereq1 = _receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended();
    bool prereq2 = _sender.stream_in().eof();
    // FIN_ACKED
    bool prereq3 = _sender.bytes_in_flight() == 0 && _sender.stream_in().bytes_written() + 2 == _sender.next_seqno_absolute();
    if (prereq1 && prereq2 && prereq3) {
        if (!_linger_after_streams_finish || _cur_time >= 10 * _cfg.rt_timeout) {
            _active_flag = false;
        }
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _send_segment();
}

void TCPConnection::connect() {
    // _sender push seg to _sender.segments_out()
    _sender.fill_window();
    // push seg to _segments_out
    _send_segment();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            _send_rst_segment();
            _active_flag = false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
