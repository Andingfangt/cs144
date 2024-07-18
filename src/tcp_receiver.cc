#include "tcp_receiver.hh"
#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <optional>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // if this message contains SYN
  if ( message.SYN ) {
    // set the zero_point
    _zero_point = message.seqno;
    // move to the right index for the first data
    message.seqno = message.seqno + 1;
    SYN = true;
  }

  // if this message contains RST, set error.
  if ( message.RST ) {
    set_error();
  }

  // only if we've recived SYN, process the data
  if ( SYN ) {
    uint64_t checkpoint = writer().bytes_pushed();
    uint64_t first_index = message.seqno.unwrap( _zero_point, checkpoint ) - 1; // not include SYN
    // byte with invalid stream index should be ignored, which means the message.seqno == _zero_point.
    if ( message.seqno == _zero_point ) {
      return;
    }
    _reassembler.insert( first_index, message.payload, message.FIN );
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  std::optional<Wrap32> ackno;
  // if we've not set the SYN, the ackno is empty
  if ( !SYN ) {
    ackno = nullopt;
  } else {
    ackno = Wrap32::wrap( writer().bytes_pushed() + 1, _zero_point ); // include SYN
    // if we assembled FIN message, which means the writer is closed, increase the ackno by 1 FIN
    if ( writer().is_closed() ) {
      ackno = ackno.value() + 1;
    }
  }
  uint16_t window_size = writer().available_capacity() > UINT16_MAX ? UINT16_MAX : writer().available_capacity();
  // use constructor to create a TCPReceiverMessage
  return { ackno, window_size, has_error() };
}
