#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::update_ackno(){
    if (_reassembler.stream_out().input_ended()) {
        _ackno = wrap(_reassembler.first_unassembled() + 2, _isn.value());
    } else {
        _ackno = wrap(_reassembler.first_unassembled() + 1, _isn.value());
    }
}

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader& header = seg.header();
    if (!_isn.has_value() || _reassembler.stream_out().input_ended()) {
        if (header.syn) {
            _isn = header.seqno;
            _reassembler.push_substring(seg.payload().copy(), 0, header.fin);   // 发起syn时如果携带数据，那么stream_idx到底是什么？？？
                                                                                // 如果此处不携带数据，那么fin=isn+1, ack=isn+2
            update_ackno();
        }
        return ;
    }

    uint64_t abs_seqno = unwrap(header.seqno, _isn.value(), _reassembler.first_unassembled());
    assert(abs_seqno != 0);
    _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, header.fin);
    update_ackno();
}

optional<WrappingInt32> TCPReceiver::ackno() const { return _ackno; }

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
