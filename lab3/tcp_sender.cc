#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { 
    uint64_t total_bytes = 0;
    for (auto it : _outstanding_segments) {
        total_bytes += it.second.length_in_sequence_space();
    }
    return total_bytes;
}

void TCPSender::fill_window() {
    /* 只有在receiver的window size不为0，且FIN并未发送的情况下，才允许发送segment。 */
    while ((_remaining_window_size > 0) && (!_fin_sent)) {
        TCPSegment segment;
        segment.header().seqno = next_seqno();
        segment.header().syn = (next_seqno_absolute() == 0);
        size_t payload_size = min(_remaining_window_size-(segment.header().syn ? 1 : 0), 
                                  TCPConfig::MAX_PAYLOAD_SIZE);
        
        /* 根据接收的window size判断是否包含EOF */
        if (_remaining_window_size > _stream.buffer_size()) {
            segment.header().fin = _stream.input_ended() ? true : false;
            _fin_sent |= segment.header().fin;
        }
        segment.payload() = _stream.read(payload_size); // 设置此segment的数据

        /* 不包含任何SYN、FIN和数据位的segment应当忽略 */
        if (segment.length_in_sequence_space() == 0) {
            break;
        }

        _outstanding_segments.emplace(next_seqno_absolute(), segment);
        _segments_out.emplace(segment);
        _retransmission_timer.start(_initial_retransmission_timeout);
        _next_seqno += segment.length_in_sequence_space();
        _remaining_window_size -= segment.length_in_sequence_space();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    uint64_t abs_ackno = unwrap(ackno, _isn, _last_abs_ackno);
    /* 检验此次ackno的合理性 */
    if ((abs_ackno < _last_abs_ackno) || 
        (abs_ackno > _last_abs_ackno+bytes_in_flight()) ||
        ((_last_abs_ackno != 0) && (abs_ackno == _last_abs_ackno) && (window_size == _last_window_size)) ) {
        return;
    }

    /* 清除已确认的 outstanding segment */
    auto it = _outstanding_segments.begin();
    while (it != _outstanding_segments.end()) {
        uint64_t abs_endno = it->first + it->second.length_in_sequence_space();
        if (abs_endno > abs_ackno) {
            break;
        }
        it = _outstanding_segments.erase(it);
    }

    /* 如果此次ACK报文指定的有效滑动窗口小于上一次的有效滑动窗口，则无需传送新的segment，直接退出函数。 */
    if ((_last_abs_ackno != 0) && 
        ((abs_ackno+window_size) == (_last_abs_ackno+_last_window_size))) {
        return;
    }
    _last_abs_ackno = abs_ackno;

    /* retransmission timer 设置 */
    _retransmission_timer.reset(_initial_retransmission_timeout);
    if (_outstanding_segments.empty()) {
        _retransmission_timer.stop();
    }
    
    /* 设置新的 _last_window_size, 并填充 receiver 的滑动窗口*/
    _last_window_size = window_size;
    _remaining_window_size = (window_size == 0) ? 1 : window_size; // 视window_size = 0为1
    fill_window(); // 填充最新的window_size
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    /**
     * 如果重传计时器返回超时，则将序列号最小的segment重传。
     * 此时若receiver返回的window size不为0，则说明可能
     * 存在网络拥堵问题，采用二进制避退算法，即将重传计时器
     * RTO加倍。
     */
    if (_retransmission_timer.expired(ms_since_last_tick)) {
        if (!_outstanding_segments.empty()) {
            _segments_out.emplace(_outstanding_segments.begin()->second);
        }
        if (_last_window_size > 0) {
            _retransmission_timer.double_rto();
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { 
    return _retransmission_timer.consecutive_retransmissions(); 
}

void TCPSender::send_empty_segment() {
    TCPSegment empty_segment;
    empty_segment.header().seqno = next_seqno();
    _segments_out.emplace(empty_segment);
}


////////////////////////////////////////////////////////////////////////////////

/**
 * RetransmissionTimer::start : 启动重传计时器，若计时器已经启动，则无视此次调用。
 * @param initial_rto : 启动重传计时器的初始RTO，仅在第一次启动时有效。
 */
void RetransmissionTimer::start(const uint16_t initial_rto) {
    /* 初次启动 */
    if (!_rto.has_value()) { 
        _ms_passed = 0;
        _rto = initial_rto;
        _consecutive_retx = 0;
    }
}

/**
 * RetransmissionTimer::stop : 关闭重传计时器，此操作会将计时器当前的一切状态清除。
 */
void RetransmissionTimer::stop() {
    if (_rto.has_value()) {
        _ms_passed = 0;
        _rto = nullopt;
        _consecutive_retx = 0;
    }
}

/**
 * RetransmissionTimer::reset : 重置重传计时器。
 * @param reset_rto : 重置的指定的RTO
 */
void RetransmissionTimer::reset(const uint16_t reset_rto) {
    _ms_passed = 0;
    _rto = reset_rto;
    _consecutive_retx = 0;
}

/**
 * RetransmissionTimer::double_rto : 将当前重传计时器的RTO翻倍
 */
void RetransmissionTimer::double_rto() {
    _ms_passed = 0;
    _rto = _rto.value() << 1; 
    ++_consecutive_retx; 
}

 /**
  * RetransmissionTimer::expired : 返回流逝时间是否已经超过设定的RTO。
  * @param  ms_since_last_check : 自从上次调用该函数时走过的时间
  * @return                     : 超过RTO返回true，否则返回false。
  */
bool RetransmissionTimer::expired(const size_t ms_since_last_check) {
    bool ret = false;
    if (_rto.has_value()) {
        _ms_passed += ms_since_last_check;
        ret = (_ms_passed >= _rto.value());
        _ms_passed = ret ? 0 : _ms_passed;
    }
    return ret; 
}

////////////////////////////////////////////////////////////////////////////////