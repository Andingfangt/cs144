#pragma once

#include "byte_stream.hh"
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <queue>
#include <unordered_set>
#include <utility>

class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output );

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return _output.reader(); }
  const Reader& reader() const { return _output.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return _output.writer(); }

  // set error to the stream
  void set_error() { _output.set_error(); }
  // return true if the stream has error
  bool has_error() const { return _output.has_error(); }

private:
  ByteStream _output;            // the Reassembler writes to this ByteStream.
  uint64_t _next_expected_index; // the index of the next byte to be written.
  struct piceData
  {
    uint64_t start_index;
    uint64_t end_index;
    std::string data;
  };
  std::deque<piceData> _buffer; // use for store bytes that can't be written yet.
  uint64_t _end_index;          // use for determine the end.
};
