#include "tcp_receiver.hh"

#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    WrappingInt32 seqno(seg.header().seqno);
    if (!_syn) {
        if (!seg.header().syn) {  // not start, ignore
            return;
        }
        _syn = true;
        _isn = seg.header().seqno;
        seqno = seqno + 1;  // if SYN, datagram seqno should plus 1
        // can't return here, segment maybe have FIN or data.
    }

    uint64_t checkpoint = _reassembler.stream_out().bytes_written();
    uint64_t absolute_index = unwrap(seqno, _isn, checkpoint);
    uint64_t stream_index = absolute_index - 1;
    _reassembler.push_substring(seg.payload().copy(), stream_index, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn) {
        return nullopt;
    }
    // plus SYN(1 byte)
    uint64_t ack_no = _reassembler.stream_out().bytes_written() + 1;

    // eof, plus FIN(1 byte)
    if (_reassembler.stream_out().input_ended()) {
        ++ack_no;
    }
    return wrap(ack_no, _isn);
}

size_t TCPReceiver::window_size() const { 
    return (_reassembler.stream_out().bytes_read() + _capacity) - _reassembler.stream_out().bytes_written();
}
