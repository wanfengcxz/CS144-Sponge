#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : ByteStream(std::vector<uint8_t>(capacity), capacity) {}

ByteStream::ByteStream(std::vector<uint8_t> vec, const size_t capacity)
    : _vec(std::move(vec)), _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    if (input_ended()) return 0;
    const size_t len = data.size();

    size_t remain_cap = remaining_capacity();
    size_t wirte_len = std::min(remain_cap, len);
    
    size_t idx = 0;
    while(idx < wirte_len) {
        _vec[_end] = static_cast<uint8_t>(data[idx++]);
        ++_used_cap;
        _end = (_end + 1)%_capacity;
    }
    _total_written += wirte_len;
    
    return wirte_len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t ret_len = std::min(len, _used_cap); 
    
    string ret;
    size_t i = 0;
    while(i<ret_len) {
        ret += static_cast<char>(_vec[(_start + i)%_capacity]);
        ++i;
    }

    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_len = std::min(len, _used_cap);
    _used_cap -= pop_len;
    _start = (_start + pop_len) % _capacity;
    _total_read += pop_len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {    
    string ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() {
    _end_input = true;
}

bool ByteStream::input_ended() const {
    return _end_input;
}

size_t ByteStream::buffer_size() const { return _used_cap; }

bool ByteStream::buffer_empty() const { return _used_cap == 0; }

bool ByteStream::eof() const { 
    if (_end_input && buffer_empty()) {
        return true;
    }
    return false;
}

size_t ByteStream::bytes_written() const { return _total_written; }

size_t ByteStream::bytes_read() const { return _total_read; }

size_t ByteStream::remaining_capacity() const { 
    return _capacity - _used_cap;
}
