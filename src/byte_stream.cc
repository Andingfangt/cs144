#include "byte_stream.hh"
#include <iostream>
#include <queue>

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : _capacity( capacity )
  , error_( false )
  , _is_closed( false )
  , _buf()
  , _pushed_bytes( 0 )
  , _buffer_bytes( 0 )
  , _popped_bytes( 0 )
{}

bool Writer::is_closed() const
{
  return _is_closed;
}

void Writer::push( string data )
{
  for ( auto c : data ) {
    // if no more capacity left, just break and throw the left data.
    if ( available_capacity() == 0 ) {
      break;
    }
    _buf.push_back(c);
    _buffer_bytes++;
    _pushed_bytes++;
  }
}

void Writer::close()
{
  _is_closed = true;
}

uint64_t Writer::available_capacity() const
{
  return _capacity - _buffer_bytes;
}

uint64_t Writer::bytes_pushed() const
{
  return _pushed_bytes;
}

bool Reader::is_finished() const
{
  // marked finished only if _is_closed && fully poped(no data in _buf)
  return _is_closed && ( _buffer_bytes == 0 );
}

uint64_t Reader::bytes_popped() const
{
  return _popped_bytes;
}

string_view Reader::peek() const
{
  // check if _buf is empty.
  if ( _buffer_bytes == 0 ) {
    return {};
  }
  // peek the first char
  return string_view( &_buf.front(), 1 );
}

void Reader::pop( uint64_t len )
{
  while ( len > 0 && _buffer_bytes > 0 ) {
    _buf.pop_front();
    _buffer_bytes--;
    _popped_bytes++;
    len--;
  }
}

uint64_t Reader::bytes_buffered() const
{
  return _buffer_bytes;
}
