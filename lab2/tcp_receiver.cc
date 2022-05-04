#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    Buffer data = seg.payload();
    /* LISTEN 处理流程 */
    if (!_isn.has_value()) {
        if (!header.syn)
            return;
        _isn = header.seqno;
    }
    /* SYN_RECV 处理流程 */
    uint64_t last_reassembled_index = stream_out().bytes_read() + stream_out().buffer_size();
    size_t stream_index = unwrap(header.seqno, _isn.value(), last_reassembled_index) - (header.syn ? 0 : 1);
    _reassembler.push_substring(data.copy(), stream_index, header.fin); // FIN_RECV 隐含在push_string中
} 

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (_isn.has_value()) {
        ByteStream inbound = stream_out();
        uint64_t abs_seqno = inbound.bytes_read() + inbound.buffer_size() + 1 + (inbound.input_ended() ? 1 : 0);
        return wrap(abs_seqno, _isn.value());
    }
    return nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
