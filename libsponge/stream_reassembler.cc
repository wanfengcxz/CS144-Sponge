#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :  StreamReassembler(capacity, std::vector<char>(capacity), std::vector<char>(capacity)){}

StreamReassembler::StreamReassembler(const size_t capacity, std::vector<char> vec, std::vector<char> flag) :
    _output(capacity), _capacity(capacity), _len(capacity), _unassembled(std::move(vec)), _flag(std::move(flag)) {}


void StreamReassembler::update_unassembled_after_read(){
    _len += _output.bytes_read() - _first_unread;   
    _first_unread = _output.bytes_read();
}

void StreamReassembler::update_unassembled_after_write(){
    _unassembled_len -= _output.bytes_written() - _first_unassembled;
    _first_unassembled = _output.bytes_written();
}

void StreamReassembler::push(size_t idx, char ch){
    _unassembled[idx] = ch;
    if (_flag[idx] != 1) ++_unassembled_len;     // 如果当前位置未有字符填充，则未重组元素个数+1
    _flag[idx] = 1;                              // 标记当前下标已经放有字符
}

void StreamReassembler::pop_front(std::string &str) {
    str += _unassembled[_start];
    _flag[_start] = 0;              // 字符被取出之后，未来会被写入重组区/读取，为了复用缓冲区，则将其设为0，即没有存储字符
    --_len;
}   

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    update_unassembled_after_read();    // 读出去了多长的字节 需要在装配缓冲区同样扩展出这段长度
   
    size_t data_len = data.size();
    if (_eof)  data_len = std::min(data_len, _end_unassembled - index - 1);             // 如果EOF已经指定过，那么需要对data进行截断，防止越过EOF
    size_t data_start = std::max(_first_unassembled, index) - index;                    // 当前index可能在_first_unassembled之前，则需要把前面的已经重组好的部分截断

    size_t offset = index > _first_unassembled?index - _first_unassembled:0;            // offset是往未重组缓冲区_unassembled写入时，下标的偏移
                                                                                        // index在_first_unassembled之后时，写入时需要加上偏移量
    while(data_start < data_len && offset < _len) {
        size_t insert_idx = (_start + offset) % _capacity;                              // _unassembled是一个循环队列，需要取模来得到下标
        push(insert_idx, data[data_start]);                                                  
        ++data_start, ++offset;
    }
    
    if (eof) {              
        _eof = true;
        _end_unassembled = std::max(index + data_len, _first_unassembled);      // TODO：如果index+data_len在_first_unassembled之前到底怎么处理？
    }

    std::string str;
    while(_len > 0 && _flag[_start] == 1) {     // 将data加入未重组缓冲区之后，可能出现连续字符串，需要write
        pop_front(str);
        _start = (_start+1)%_capacity;
    }
    
    _output.write(str);
    update_unassembled_after_write();
    if (_eof && _first_unassembled >= _end_unassembled) _output.end_input();        // 此次写入的连续字符串刚好包含了EOF位置，则表示数据接收完毕。
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_len; }

bool StreamReassembler::empty() const { return _len == _capacity; }
