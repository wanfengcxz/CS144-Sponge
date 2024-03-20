#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// static void print(string name, uint64_t v){
//     cerr << name << ":" << v << endl;
// }

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    /*
        |__________________________|__________________________|_________________________|
        1                          2                          3                         4
        _old_next_seqno            _new_next_seqno            _old_end                  _new_end

        _old_next_seqno + old_window_size = _old_end
        _new_next_seqno + new_window_size = _new_end

        此时，_bytes_in_flight记录了发出的数据，即部分或全部abs_seqno在[2,3]之间的数据段。
        接下来需要把[3,4]之间的数据打包发出去
    */
    // testcase{send_extra.cc}: When filling window, treat a '0' window size as equal to '1' but don't back off RTO
    uint16_t window_size = std::max(static_cast<uint16_t>(1), _window_size);  // 如果window_size=0，需要设置为1
    while (_bytes_in_flight < window_size) {
        
        // 封装TCP数据包
        TCPSegment seg;
        if (!_send_syn_flag) {  // 如果没有发送过syn，则发送一次syn
            // 如果没有发送过syn，则发送一次syn。发送完syn，_next_seqno+=1
            // 如果接收到了，会在ack_received将这个seg出队
            seg.header().syn = true;
            _send_syn_flag = true;
        }
        
        size_t payload_size =
            min(TCPConfig::MAX_PAYLOAD_SIZE,
                min(_stream.buffer_size(),
                    window_size - _bytes_in_flight - seg.length_in_sequence_space()));  // 要兼顾最大长度，当前还差的长度，stream中的最大长度
                                                       // 初始化window_size=1，此处payload_size=0，不会携带任何数据

        seg.payload() = Buffer(std::move(_stream.read(payload_size)));
        seg.header().seqno = wrap(_next_seqno, _isn);

        // 必须放在封装数据包下面，因为发送fin时可以携带数据
        // testcase{send_extra.cc} {"Don't add FIN if this would make the segment exceed the receiver's window", cfg}
        // 所以需要加上判断最后一个判断条件
        if (_stream.eof() && !_send_fin_flag && _bytes_in_flight + seg.length_in_sequence_space() < window_size) {
            // 如果没有发送过fin，则发送一次fin。发送完fin，_next_seqno+=1
            // 如果接收到了，会在ack_received将这个seg出队
            seg.header().fin = true;
            _send_fin_flag = true;
        }

        size_t send_len = seg.length_in_sequence_space();
        // cerr << send_len << endl;
        if (send_len == 0)
            break;  // 空包 没有数据可以发送过去 防止死循环

        // 发送TCP数据包
        _segments_out.push(seg);
        _segments_outstanding.emplace(_next_seqno, std::move(seg));  // 最好使用emplace，能够同时转发左值和右值

        if (!_timer.state())
            _timer.reset();

        _next_seqno += send_len;
        // cerr << "send_len" << send_len << endl;
        // cerr << "_next_seqno" << _next_seqno << endl;
        _bytes_in_flight += send_len;
        if (_stream.input_ended() && _stream.buffer_size() == 0)
            break;  // 数据全部发送完毕

        // print("_bytes_in_flight", _bytes_in_flight);
        // std::abort();
    }

    // cerr << _next_seqno << endl;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    /*
        |__________________________|__________________________|_________________________|
        1                          2                          3                         4
        _old_next_seqno            _new_next_seqno            _old_end                  _new_end

        _old_next_seqno + old_window_size = _old_end    大约
        _new_next_seqno + new_window_size = _new_end    大约

        当某次传输时，接收到了新的ack，即_new_next_seqno
        此时[1,3]内的数据都已经分段放到了_segments_outstanding
        1. 首先丢弃要把[1,2]中abs_seq全部都小于_new_next_seqno的数据段
        2. 尽可能填充新的windos。即把[2,4]的数据发给receiver，但是此时[2,3]内的数据已经发出去了，
        并暂存在_segments_outstanding，因此他们会自动超时重传。所以只需要把[3,4]内的数据发出去，
        并暂存在_segments_outstanding中，同时更新byte_stream，把已发出的数据丢掉。
    */

    uint64_t ack_abs_seqno = unwrap(ackno, _isn, _next_seqno);
    if (ack_abs_seqno > next_seqno_absolute())
        return;  // 传入的ack大于已经发出的数据 不合法

    // 将队列中abs_seqno小于ack_abs_seqno的数据段扔掉
    bool is_receive_seg = false;
    while (_segments_outstanding.size()) {
        auto &[t_abs_seqno, t_seg] = _segments_outstanding.front();
        uint64_t t_end_abs_seqno = t_abs_seqno + t_seg.length_in_sequence_space();
        if (t_end_abs_seqno > ack_abs_seqno)
            break;
        _bytes_in_flight -= t_seg.length_in_sequence_space();
        _segments_outstanding.pop();
        is_receive_seg = true;
    }

    if (is_receive_seg) {  // 如果有发送成功的报文
        _consecutive_retransmissions_count = 0;
        _timer.set_time_out(_initial_retransmission_timeout);
        _timer.reset();
    }

    _window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer.state())
        return;

    _timer.tick(ms_since_last_tick);
    if (_timer.time_out() && _segments_outstanding.size()) {
        // 重传最早的报文
        _segments_out.push(_segments_outstanding.front().second);
        // testcase{send_extra.cc}: When filling window, treat a '0' window size as equal to '1' but don't back off RTO
        // 如果window_size为0，那么不会倍增time_out
        if (_window_size > 0) {
            ++_consecutive_retransmissions_count;
            _timer.set_time_out(_timer.get_time_out() * 2);
        }
        _timer.reset();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().ack = true;
    seg.header().seqno = next_seqno();  // 为什么是_next_seqno
    _segments_out.emplace(std::move(seg));
}
