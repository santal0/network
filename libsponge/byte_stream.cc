#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) 
      : buffer_(),          // 初始化空字符串
        capacity_(capacity), // 初始化容量为传入的参数
        bytes_written_(0),  // 初始总共写入字节数为0
        bytes_read_(0),     // 初始总共读取字节数为0
        input_ended_(false) // 初始输入未结束
  { 
      DUMMY_CODE(capacity); // 保留原有占位代码（如果需要）
  }


// 写入数据：最多写入剩余容量的字节
size_t ByteStream::write(const string &data) {
    if (input_ended_) return 0;  // 输入已结束，不能再写入

    size_t max_write = remaining_capacity();
    size_t actual_write = min(data.size(), max_write);
    if (actual_write > 0) {
        buffer_.append(data.substr(0, actual_write));
        bytes_written_ += actual_write;
    }
    return actual_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
// 查看缓冲区头部数据
string ByteStream::peek_output(const size_t len) const {
    size_t peek_len = min(len, buffer_size());
    return buffer_.substr(0, peek_len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
// 弹出缓冲区头部数据
void ByteStream::pop_output(const size_t len) {
    if (len == 0) return;
    size_t pop_len = min(len, buffer_size());
    buffer_.erase(0, pop_len);
    bytes_read_ += pop_len;
}


//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
// 读取并弹出数据
string ByteStream::read(const size_t len) {
    string result = peek_output(len);
    pop_output(result.size());
    return result;
}

// 标记输入结束
void ByteStream::end_input() {
    input_ended_ = true;
}

// 检查输入是否结束
bool ByteStream::input_ended() const {
    return input_ended_;
}

// 当前缓冲区大小
size_t ByteStream::buffer_size() const {
    return buffer_.size();
}

// 缓冲区是否为空
bool ByteStream::buffer_empty() const {
    return buffer_.empty();
}

// 是否达到 EOF（输入结束且缓冲区为空）
bool ByteStream::eof() const {
    return input_ended_ && buffer_empty();
}

// 累计写入字节数
size_t ByteStream::bytes_written() const {
    return bytes_written_;
}

// 累计读取字节数
size_t ByteStream::bytes_read() const {
    return bytes_read_;
}

// 剩余容量
size_t ByteStream::remaining_capacity() const {
    return capacity_ - buffer_size();
}