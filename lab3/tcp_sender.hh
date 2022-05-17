#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>
#include <map>


/**
 * 简易的重传计时器
 */
class RetransmissionTimer {
private:
  // 记录每一次RTO期间流逝了多少的时间，在计时器重置、超时以及加倍RTO时被置0。
  unsigned int _ms_passed{0};

  // 记录每一次RTO
  std::optional<unsigned int> _rto{};

  // 计时器重传的次数，即RTO加倍的次数。
  unsigned int _consecutive_retx{0};

public:
  // 启动重传计算器
  void start(const uint16_t initial_rto);

  // 关闭重传计算器
  void stop();

  // 重置重传计算器
  void reset(const uint16_t reset_rto);
  
  // 加倍RTO
  void double_rto();

  // 判断计算器是否超时
  bool expired(const size_t ms_since_last_check);

  // 获取重传的次数
  unsigned int consecutive_retransmissions() const { return _consecutive_retx; }

};


//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    // 重传计时器
    RetransmissionTimer _retransmission_timer{};

    // 利用map自动排序的特性管理未被确认的segment
    std::map<uint64_t, TCPSegment> _outstanding_segments{};

    // 上一次(最新)接收到的有效的ACK绝对序号，初始为0
    uint64_t _last_abs_ackno{0};

    // 上一次(最新)接收到的有效的滑动窗口大小，初始时因为需要发起SYN请求，视接收方滑动窗口为1
    size_t _last_window_size{1};

    // 上一次(最新)接收到的有效的滑动窗口剩余大小，记录调用fillwindow后剩余的接收方滑动窗口大小
    size_t _remaining_window_size{1};

    // 记录FIN标志是否已经发送，用于防止重复地发送FIN。
    bool _fin_sent{false};

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};
#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
