#include "byte_stream.hh"

#include <cmath>
#include <iostream>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _bytes(string{})
    , _capacity(capacity)
    , _read_index(0)
    , _write_index(0)
    , _read(0)
    , _written(0)
    , _input_ended(false)
    , _error(false) {
    if (_capacity == 0) {
        cerr << "capacity can't is 0" << endl;
        exit(0);
    }
}

size_t ByteStream::write(const string &data) {
    if (_input_ended == true) {
        return 0;
    }

    size_t max_write = _capacity - (_write_index - _read_index);
    if (data.length() <= max_write) {
        _bytes += data;
        _write_index += data.length();
        _written += data.length();
        return data.length();
    } else {
        _bytes += data.substr(0, max_write);
        _write_index += max_write;
        _written += max_write;
        return max_write;
    }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t max_read = _write_index - _read_index;
    return _bytes.substr(_read_index, min(max_read, len));
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t max_read = _write_index - _read_index;
    _read_index += min(max_read, len);
    _read += min(max_read, len);
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
string ByteStream::read(const size_t len) {
    auto read_ret = peek_output(len);
    pop_output(len);
    trimBytes();
    return read_ret;
}

void ByteStream::end_input() { _input_ended = true; }

bool ByteStream::input_ended() const { return _input_ended == true; }

size_t ByteStream::buffer_size() const { return _write_index - _read_index; }

bool ByteStream::buffer_empty() const { return _write_index == _read_index; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _written; }

size_t ByteStream::bytes_read() const { return _read; }

size_t ByteStream::remaining_capacity() const {
    return _capacity - (_write_index - _read_index);
}

void ByteStream::trimBytes() {
    if (_bytes.length() > static_cast<size_t>(ceil(1.5 * _capacity))) {
        _bytes = _bytes.substr(_read_index, _bytes.length() - _read_index);
        _write_index -= _read_index;
        _read_index -= _read_index;
    }
}
