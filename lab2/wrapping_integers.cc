#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return WrappingInt32(isn + static_cast<uint32_t>(n));
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    /**
     * 根据 int32_t operator-(WrappingInt32 a, WrappingInt32 b) 可以求得在seqno space
     * 两个WrappingInt32的**最短**距离, 因为int32_t = uint32_t - uint32_t的结果为
     * 两个无符号整数之间的最短距离的绝对值，符号表示方向。
     * 而形如int64_t = uint64_t + int32_t 的式子会将结果
     * 从uint64_t表示的范围'0 ~ 2^64 - 1'平移到int64_t表示的'-2^32 ~ 2^32 -1'的范围
     * 因而造成下溢(negative overflow)
     */
    int32_t offset = n - wrap(checkpoint, isn);
    int64_t result = checkpoint + offset;
    return result >= 0 ? result : result + (1ul << 32);
}
