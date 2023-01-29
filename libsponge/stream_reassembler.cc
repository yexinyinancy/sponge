#include "stream_reassembler.hh"
#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _first_unread(0), _first_unassemble(0), _end(false), _end_ind(-1) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t f_unaccep = _first_unread + _capacity;
    if (eof && (index + data.size()) <= f_unaccep) {
        _end_ind = index + data.size();
        _end = true;
    }
    size_t end = min(f_unaccep, index + data.size());
    if (_end) {
        end = min(end, _end_ind);
    }
    size_t start = max(_first_unassemble, index);
    for (size_t i = 0; i < data.size(); i++) {
        if (i + index >= start && i + index < end) {
            if (_map.find(i + index) == _map.end()) {
                _map[i + index] = data[i];
            }
        }
    }
    while (_map.find(_first_unassemble) != _map.end()) {
        _first_unassemble++;
    }
    size_t to_stream_len = min(_first_unassemble - _first_unread, _output.remaining_capacity());
    string s;
    for (size_t i = 0; i < to_stream_len; i++) {
        s += _map[i + _first_unread];
        _map.erase(i + _first_unread);
    }
    _first_unread += to_stream_len;
    _output.write(s);
    if (_end && _first_unread == _end_ind) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _map.size() - (_first_unassemble - _first_unread); }

bool StreamReassembler::empty() const { return _map.size() == 0 && _output.buffer_empty(); }
