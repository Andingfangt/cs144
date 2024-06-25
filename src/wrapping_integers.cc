#include "wrapping_integers.hh"
#include <cmath>
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // convert uint64_t n to a 32-bit unsigned integer, wrapping by 2^32
  // uint32_t wrapped_n = n % (1ULL << 32);
  uint32_t wrapped_n = static_cast<uint32_t>( n );
  // use provide overwrite + operator.
  return zero_point + wrapped_n;
}

/* use for compute diff between two uint64_t number*/
uint64_t abs_diff( uint64_t a, uint64_t b )
{
  return a > b ? a - b : b - a;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // it's sure that this._raw_value is geq zero_point._raw_value
  uint64_t n = this->_raw_value - zero_point._raw_value;

  // there are three possible cases for n:
  // add the first 32 bits of checkpoint to n
  uint64_t n1 = n + ( checkpoint & 0xFFFFFFFF00000000 );
  uint64_t n2 = n1 + ( 1ULL << 32 );
  uint64_t n3 = n1 - ( 1ULL << 32 );

  // compute the absolute difference between n and checkpoint for each case
  uint64_t diff1 = abs_diff( n1, checkpoint );
  uint64_t diff2 = abs_diff( n2, checkpoint );
  uint64_t diff3 = abs_diff( n3, checkpoint );

  // return the closest one to the checkpoint
  return diff1 <= diff2 && diff1 <= diff3 ? n1 : diff2 <= diff1 && diff2 <= diff3 ? n2 : n3;
}
