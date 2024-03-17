#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <vector>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  public:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes

    size_t _first_unread{};
    size_t _first_unassembled{};

    size_t _start{};
    size_t _len;
    size_t _unassembled_len{};
    std::vector<char> _unassembled;
    std::vector<char> _flag;
    size_t _str_len{};
    std::string _str{};

    bool _eof{};
    size_t _end_unassembled{};
    
    inline void update_unassembled_after_read();

    inline void update_unassembled_after_write();
    
    //! \brief Push a char into `_unassembled`, update `_flag` and `un_assembled_len`
    inline void push(size_t idx, char ch);

    //! \brief Pop a char at front and append it to the end of str, update `_flag` and `_len` 
    inline void pop_front(std::string &str);

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    StreamReassembler(const size_t capacity, std::vector<char> vec, std::vector<char> flag);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    size_t first_unassembled() const {
      return _first_unassembled;
    }

    size_t unassembled_buffer_len() const {
      return _len;
    }
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
