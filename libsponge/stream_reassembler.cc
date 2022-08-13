#include "stream_reassembler.hh"

#include <iostream>
#include <algorithm>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), 
                                                              _capacity(capacity),
                                                              _vis(set<pair<size_t, size_t>>{}),
                                                              _buffer(string{}),
                                                              _assembled(0),
                                                              _eof(false),
                                                              _eof_index(0) {
    if (_capacity == 0) {
        cerr << "capacity can't is 0" << endl;
        exit(0);
    }
    // fixed string && clear set
    _buffer.resize(capacity);
    _vis.clear();
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (data.length() == 0 || index + data.length() < min_buffer_index() || index > max_buffer_index()) {
        handle_eof(eof, index + data.length());
        return ;
    }

    insert_buffer(data, index);

    handle_eof(eof, index + data.length());
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t sum = 0;
    for (auto &&i : _vis) {
        sum += (i.second - i.first);
    }
    return sum;
}

bool StreamReassembler::empty() const { return _vis.size() == 0; }

void StreamReassembler::insert_buffer(const string &data, const uint64_t index) {
    pair<size_t, size_t> insert_pr = make_pair(index, index + data.length());
    // TODO bug here
    size_t cha = 0;
    if (index < min_buffer_index()) {
        insert_pr.first = min_buffer_index();
        cha = min_buffer_index() - index;
    }
    if (index + data.length() > max_buffer_index()) {
        insert_pr.second = max_buffer_index();
    }
    // cout << insert_pr.first << " " << insert_pr.second << endl;
    if (_vis.find(insert_pr) != _vis.end()) {
        return ;
    }

    _vis.insert(insert_pr);

    // merge
    auto cur_pr = forward_merge(insert_pr);
    cur_pr = backward_merge(cur_pr);

    size_t len = insert_pr.second - insert_pr.first;
    for (size_t i = 0; i < len; i++) {
        _buffer[insert_pr.first + i - min_buffer_index()] = data[i + cha];
        // cout << data[i + cha] << " " << _buffer << endl;
    }

    if (_vis.begin()->first == min_buffer_index()) {
        size_t apply_len = _vis.begin()->second - _assembled;
        // cout << _vis.begin()->first << " " << _vis.begin()->second << " " << _buffer << endl;
        // cout << "write to buffer: " << _buffer.substr(0, apply_len) << " " << _assembled << endl;
        _output.write(_buffer.substr(0, apply_len));
        _buffer = _buffer.substr(apply_len);
        _buffer.resize(_capacity);
        _assembled = _vis.begin()->second;
        // cout << "_assembled: " << _assembled << endl;
        _vis.erase(_vis.begin());
    }
}

std::pair<size_t, size_t> StreamReassembler::forward_merge(std::pair<size_t, size_t> insert_pr) {
    auto cur_iter = _vis.find(insert_pr);
    if (cur_iter == _vis.begin()) { // don't need forward_merge
        return *cur_iter;
    }

    auto prev_iter = set<pair<size_t, size_t>>::reverse_iterator(cur_iter);
    while (prev_iter != _vis.rend()) {
        if (prev_iter->second < cur_iter->first) {
            break;
        }
        // merge
        size_t left = min(prev_iter->first, cur_iter->first);
        size_t right = max(prev_iter->second, cur_iter->second);
        auto erase_tmp_iter = prev_iter;
        prev_iter++;
        _vis.erase(*erase_tmp_iter);
        _vis.erase(*cur_iter);
        cur_iter = _vis.insert({left, right}).first;
    }
    return *cur_iter;
}

std::pair<size_t, size_t> StreamReassembler::backward_merge(std::pair<size_t, size_t> insert_pr) {
    auto cur_iter = _vis.find(insert_pr);
    if (cur_iter == (--_vis.end())) { // don't need backward_merge
        return *cur_iter;
    }

    auto next_iter = cur_iter++;
    while (next_iter != _vis.end()) {
        if (cur_iter->second < next_iter->first) {
            break;
        }
        // merge
        size_t left = min( cur_iter->first, next_iter->first);
        size_t right = max(cur_iter->second, next_iter->second);
        auto erase_tmp_iter = next_iter;
        next_iter++;
        _vis.erase(*erase_tmp_iter);
        _vis.erase(*cur_iter);
        cur_iter = _vis.insert({left, right}).first;
    }
    return *cur_iter;
}

size_t StreamReassembler::max_buffer_index() {
    return _assembled + _capacity - _output.buffer_size();
}

size_t StreamReassembler::min_buffer_index() {
    return _assembled;
}

void StreamReassembler::handle_eof(const bool eof, const size_t eof_index) {
    if (!_eof && eof) { // just once
        _eof = true;
        _eof_index = eof_index;
        // cout << "eof index: " << _eof_index << endl;
    }
    // cout << "assembled: " << _assembled << " eof_index: " << _eof_index << endl;
    if (_eof && _assembled >= _eof_index) {
        // cout << "-----eof!" << endl;
        _output.end_input();
        return ;
    }
}
