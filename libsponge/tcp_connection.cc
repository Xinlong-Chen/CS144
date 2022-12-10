#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_active) {
        return;
    }

    _time_since_last_segment_received = 0;

    if (seg.header().rst) {
        close_connection();
        return;
    }

    _receiver.segment_received(seg);

    // recieve SYN (3-way-wave, step 2)
    if (seg.header().syn && _sender.next_seqno_absolute() == 0) {
        connect();
        return;  // don't need to deal with ack
    }

    // cerr << "_sender.next_seqno_absolute(): " << _sender.next_seqno_absolute() << endl;
    // handle ack which I have sent (also 3-way-handshake, step 3, 4-way handshake, step 3)
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);  // maybe fill_windows here
    }

    // recieve FIN (4-way handshake, step 2)
    if (seg.header().fin && _sender.next_seqno_absolute() < _sender.stream_in().bytes_written() + 2) {
        _sender.fill_window();
    }

    // 判断 TCP 断开连接时是否时需要等待
    // CLOSE_WAIT
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
        _linger_after_streams_finish = false;

    // 如果到了准备断开连接的时候。服务器端先断
    // CLOSED
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
        _active = false;
        return;
    }

    bool send_empty = false;
    auto ackno = _receiver.ackno();

    // "at least once" ack
    if (ackno.has_value() && seg.length_in_sequence_space() > 0 && _sender.segments_out().empty()) {
        _sender.fill_window();
        if (_sender.segments_out().empty()) {
            send_empty = true;
        }
    }

    // keep-alive
    if (ackno.has_value() && seg.length_in_sequence_space() == 0 && seg.header().seqno == ackno.value() - 1) {
        send_empty = true;
    }

    if (send_empty)
        _sender.send_empty_segment();

    send_TCPSegments();
}

size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    send_TCPSegments();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (ms_since_last_tick == 0) {
        return;
    }

    _time_since_last_segment_received += ms_since_last_tick;
    // cerr << "TIME NOW:" << _time_since_last_segment_received << " TIME PASS: " << ms_since_last_tick << endl;

    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        reset_connection();  // end connection
        return;
    }

    // send ack with timer
    send_TCPSegments();

    // 如果处于 TIME_WAIT 状态并且超时，则可以静默关闭连接
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish &&
        _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {  // (4-way handshake, step 1)
    _sender.stream_in().end_input();
    _sender.fill_window();  // fill with FIN
    send_TCPSegments();
}

void TCPConnection::connect() {  // (3-way-handshake, step 1)
    _sender.fill_window();       // fill with SYN
    send_TCPSegments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            reset_connection();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_TCPSegments() {
    while (!_sender.segments_out().empty()) {
        TCPSegment segment = _sender.segments_out().front();
        _sender.segments_out().pop();

        auto ackno = _receiver.ackno();
        if (ackno.has_value()) {
            segment.header().ack = true;
            segment.header().ackno = ackno.value();
            segment.header().win = _receiver.window_size();
        }

        _segments_out.push(segment);
    }

}

void TCPConnection::is_end() { }

void TCPConnection::reset_connection() {
    close_connection();
    send_rst();
}

void TCPConnection::close_connection() {
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false;
}

void TCPConnection::send_rst() {
    _sender.send_empty_segment();
    TCPSegment segment = _sender.segments_out().front();
    _sender.segments_out().pop();
    segment.header().rst = true;
    _segments_out.push(segment);
}
