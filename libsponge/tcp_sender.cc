#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {
    _timer.reset_all(retx_timeout);
}

void TCPSender::fill_window() {
    if (_fin) {  // eof
        return;
    }

    if (!_syn) {  // begin, send SYN to tcp peer
        send_segment(SYN);
        _syn = true;
        return;
    }

    uint16_t window_size = _receiver_window_size > 0 ? _receiver_window_size : 1;
    if (_stream.eof() && _recv_seqno + window_size > _next_seqno) {
        send_segment(FIN);
        _fin = true;
        return;
    }

    TCPSegment segment;
    while (!_stream.buffer_empty() && _recv_seqno + window_size > _next_seqno) {
        size_t send_size =
            min(TCPConfig::MAX_PAYLOAD_SIZE, static_cast<size_t>(window_size - (_next_seqno - _recv_seqno)));
        segment.payload() = _stream.read(min(send_size, _stream.buffer_size()));

        if (_stream.eof() && segment.length_in_sequence_space() < window_size) {
            segment.header().fin = true;
            _fin = true;
        }
        send_segment(NORMAL, move(segment));
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t ack_no = unwrap(ackno, _isn, _recv_seqno);
    if (ack_no < _recv_seqno || ack_no > _next_seqno) {
        // overdue ack or overflow window, ignore
        return;
    }
    _recv_seqno = ack_no;
    _receiver_window_size = window_size;

    // bool is_pop = false;

    while (!_segments_waiting.empty()) {
        TCPSegment segment = _segments_waiting.front();
        uint64_t seq_no = unwrap(segment.header().seqno, _isn, _recv_seqno);
        // cout << "ack_no: " << ack_no << " seq_no: " << seq_no + segment.length_in_sequence_space() << endl;
        if (ack_no < seq_no + segment.length_in_sequence_space()) {
            break;
        }
        _bytes_in_flight -= segment.length_in_sequence_space();
        _segments_waiting.pop();
        // is_pop = true;
        _timer.reset_all(_initial_retransmission_timeout);
    }

    if (_next_seqno - _recv_seqno < _receiver_window_size) {
        fill_window();
    }

    if (!_timer.is_start()) {
        // cout << "start4" << endl;
        _timer.start();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // cout << "timer status: " << _timer.is_start() << endl;
    if (!_timer.is_start()) {
        return;
    }

    // cout << "_segments_waiting.empty(): " << _segments_waiting.empty() << endl;

    if (_timer.tick(ms_since_last_tick) && !_segments_waiting.empty()) {  // time out
        // re-send the first segment in _segments_waiting
        auto segment = _segments_waiting.front();
        // cout << "tick: re-send " << segment.header().seqno << endl;
        send_segment(RESEND, move(segment));
        // cout << "_receiver_window_size: " << _receiver_window_size << endl;
        if (_receiver_window_size > 0 || segment.header().syn) {
            _timer.reset();
            // cout << "start1" << endl;
            _timer.start();
        } else if (_receiver_window_size == 0) {
            _timer.reset(false);
            _timer.start();
        } 
    }
}

void TCPSender::send_empty_segment() { send_segment(EMPTY); }

void TCPSender::send_segment(SegmentType type, TCPSegment &&segment) {
    if (type == RESEND) {
        _segments_out.push(segment);
        if (!_timer.is_start()) {
            // cout << "start2" << endl;
            _timer.start();
        }
        return;
    }

    segment.header().seqno = wrap(_next_seqno, _isn);
    if (type == EMPTY) {
        _segments_out.push(segment);
        return;
    }

    switch (type) {
        case SYN:
            segment.header().syn = true;
            break;
        case FIN:
            segment.header().fin = true;
            break;
        default:
            break;
    }

    _segments_out.push(segment);
    _segments_waiting.push(segment);
    _next_seqno += segment.length_in_sequence_space();
    _bytes_in_flight += segment.length_in_sequence_space();

    // cout << "timer is_start: " << _timer.is_start() << endl;
    if (!_timer.is_start()) {
        // cout << "start3" << endl;
        _timer.start();
    }
}
