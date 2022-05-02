#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) 
    : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    /* 动态地获取关键index值 */
    size_t first_unread_index = _output.bytes_read();
    size_t first_unassembled_index = first_unread_index + _output.buffer_size();
    size_t first_unacceptable_index = first_unread_index + _capacity;
    size_t data_len = data.length();
    /* 保证子串在[first_unassembled_index, first_unacceptable_index)区间内 */
    if ((index+data_len < first_unassembled_index) || 
        (index >= first_unacceptable_index)) {
        return;
    } else if (data_len == 0) { // 传入data为空时的加速处理
        _eof |= eof; // 处理为EOF的碎片
        if (empty() && eof) {  
            _output.end_input();
        }
        return;
    }

    /* 计算实际能重组的子串长度，裁剪子串("掐头去尾") */ 
    size_t substr_begin_pos, substr_len, new_index = index;
    // 掐头
    if (index < first_unassembled_index) { 
        substr_begin_pos = first_unassembled_index-index;
        new_index = first_unassembled_index; // 去除头部部分数据，改变index
    } else {
        substr_begin_pos = 0;
    }
    // 去尾
    if (index+data_len <= first_unacceptable_index) {
        substr_len = data_len-substr_begin_pos;
        _eof |= eof; // 尾部数据全部有效，即最后一个字节数据有效，根据eof改变_eof值
    } else {
        size_t overflow_bytes = index+data_len - first_unacceptable_index;
        substr_len = data_len-substr_begin_pos - overflow_bytes;
    }
    string segment(data, substr_begin_pos, substr_len);

    /* 利用map特性，合并重组子串 */
    size_t tmp_index = new_index;
    for (auto it : segment) {
        _unassembled_segments.emplace(tmp_index, it);
        ++tmp_index;
    }

    /* push重组的片段到_output中 */ 
    string pushed_segment;
    if (new_index == first_unassembled_index) {
        auto it = _unassembled_segments.begin();
        while ((it != _unassembled_segments.end()) && (it->first == new_index)) {
            pushed_segment.push_back(it->second);
            _unassembled_segments.erase(it++);
            ++new_index;
        }
        _output.write(pushed_segment);
    }

    if (_eof && empty())
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_segments.size(); }

bool StreamReassembler::empty() const { return _unassembled_segments.empty(); }
