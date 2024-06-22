#include "reassembler.hh"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace std;

Reassembler::Reassembler( ByteStream&& output )
  : _output( std::move( output ) ), _next_expected_index( 0 ), _buffer(), _end_index( UINT64_MAX )
{}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  /*first, add all valid bytes to the buffer.*/
  // if current index is less than _next_need_index, skip it.
  uint64_t begin_index = max( first_index, _next_expected_index );
  // if current index is out of range, break the loop
  uint64_t max_valid_index
    = min( _output.writer().available_capacity() + _next_expected_index, first_index + data.size() );
  for ( uint64_t curr_index = begin_index; curr_index < max_valid_index; ++curr_index ) {
    // no need to check overlapping since we use map.
    _buffer[curr_index] = data[curr_index - first_index];
  }

  // add all valid to writer
  auto it = _buffer.begin();
  string push_data = "";
  while ( !_buffer.empty() && it->first == _next_expected_index ) {
    push_data += it->second;
    _next_expected_index++;
    it = _buffer.erase( it ); // original map is soreted, and erase will return an iterator to the next element.
  }
  _output.writer().push( push_data );

  // check if all bytes have been written to the output stream
  if ( is_last_substring ) {
    _end_index = first_index + data.size();
  }
  if ( _next_expected_index == _end_index ) {
    _output.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return _buffer.size();
}
