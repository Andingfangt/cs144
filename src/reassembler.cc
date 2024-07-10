#include "reassembler.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ratio>
#include <stdexcept>
#include <utility>

using namespace std;

Reassembler::Reassembler( ByteStream&& output )
  : _output( std::move( output ) ), _next_expected_index( 0 ), _buffer(), _end_index( UINT64_MAX )
{}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // !!! do not use std::map, it is slow for multipul insert and erase operations.

  // if this is the last substring, update _end_index
  if ( is_last_substring ) {
    _end_index = first_index + data.size();
  }

  // if current index is less than _next_need_index, skip it.
  uint64_t start_index = max( first_index, _next_expected_index );
  // if current index is out of range, break the loop
  uint64_t end_index = min( writer().available_capacity() + _next_expected_index, first_index + data.size() ) - 1;
  // if the valid range is empty, no need to insert
  if ( end_index < start_index || data.empty() )
    goto close_check;

  // ** first, insert all data into buffer with mergy **
  {
    data = data.substr( start_index - first_index, end_index - start_index + 1 );
    piceData curr_data = { start_index, end_index, move( data ) };
    // if find a subData.end_index < curr_data.start, means no need to merge and continue.
    // so use binary search to find the first subData that its end_index >= curr_data.start_index.
    auto it = lower_bound( _buffer.begin(), _buffer.end(), curr_data, []( const piceData& a, const piceData& b ) {
      return a.end_index < b.start_index;
    } );
    // loop all inside buffer_data to compress the buffer by merge
    while ( it != _buffer.end() ) {
      // if find a subData that is overlaped by curr_data, just remove it and continue the loop
      if ( it->start_index >= curr_data.start_index && it->end_index <= curr_data.end_index ) {
        it = _buffer.erase( it );
        continue;
      }

      // if find a subData that overlaps curr_data. no need to do anything.
      if ( it->start_index <= curr_data.start_index && it->end_index >= curr_data.end_index ) {
        goto close_check;
      }

      // if find a subData.start_index > curr_data.end_index, break the loop
      if ( it->start_index > curr_data.end_index ) {
        break;
      }

      // left is two cases:
      // case1: subData.end_index < curr_data.end_index, merge subData to curr_data's left
      if ( it->end_index < curr_data.end_index ) {
        string add_data = it->data.substr( 0, curr_data.start_index - it->start_index );
        curr_data.start_index = it->start_index;
        curr_data.data.insert( 0, move( add_data ) );
      }
      // case2: subData.start_index <= curr_data.end_index, merge subData to curr_data's right
      else {
        string add_data
          = it->data.substr( curr_data.end_index - it->start_index + 1, it->end_index - curr_data.end_index );
        curr_data.end_index = it->end_index;
        curr_data.data.append( move( add_data ) );
      }
      // remove the subData from buffer
      it = _buffer.erase( it );
    }
    // insert curr_data into buffer
    _buffer.emplace( it, move( curr_data ) );

    // ** then, write valid data from buffer to output stream **
    it = _buffer.begin();
    while ( it != _buffer.end() && it->start_index == _next_expected_index ) {
      _output.writer().push( move( it->data ) );
      _next_expected_index = it->end_index + 1;
      it = _buffer.erase( it );
    }
  }

// check if all bytes have been written to the output stream
close_check:
  if ( _next_expected_index == _end_index ) {
    _output.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // return the total size in buffer
  uint64_t total_size = 0;
  for ( const auto& subData : _buffer ) {
    total_size += subData.data.size();
  }
  return total_size;
}
