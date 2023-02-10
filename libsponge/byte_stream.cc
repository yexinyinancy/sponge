#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity), _read_cnt(0), _write_cnt(0), _end_input(false) {}

size_t ByteStream::write(const string &data) {
    size_t copy_len = min(data.size(), _capacity - _buf.size());
    _write_cnt += copy_len;
    for (size_t i = 0; i < copy_len; i++) {
        _buf.push_back(data[i]);
    }
    return copy_len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_len = min(_buf.size(), len);
    std::string ret(_buf.begin(), _buf.begin() + peek_len);
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t peek_len = min(_buf.size(), len);
    _read_cnt += peek_len;
    for (size_t i = 0; i < peek_len; i++) {
        _buf.pop_front();
    }
    return;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _buf.size(); }

bool ByteStream::buffer_empty() const { return _buf.size() == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _write_cnt; }

size_t ByteStream::bytes_read() const { return _read_cnt; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buf.size(); }
