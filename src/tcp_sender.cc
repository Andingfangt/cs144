#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cmath>
#include <cstdint>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return _outstanding_sequence_numbers;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return _consecutive_retransmissions_times;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // while still can send messages
  while ( _outstanding_sequence_numbers < _window_size ) {
    TCPSenderMessage msg;
    // if have not send SYN
    if ( !_has_send_SYN ) {
      msg.SYN = true;
      _has_send_SYN = true;
    }
    // set the sequence number with current _checkpoint
    msg.seqno = Wrap32::wrap( _abs_seq, isn_ );

    // get the biggest len of payload, which is the minimun of (TCPConfig::MAX_PACKET_SIZE, _window_size -
    // _outstanding_sequece_number, ByteSteam)
    size_t payload_len = min( TCPConfig::MAX_PAYLOAD_SIZE,
                              min( _window_size - _outstanding_sequence_numbers, reader().bytes_buffered() ) );
    read( input_.reader(), payload_len, msg.payload );
    _outstanding_sequence_numbers += msg.sequence_length();

    // if have read all the bytes in reader, check if have send FIN and it is availible to send it
    if ( reader().is_finished() && !_has_send_FIN && _outstanding_sequence_numbers < _window_size ) {
      msg.FIN = true;
      _has_send_FIN = true;
      _outstanding_sequence_numbers++;
    }

    // if the msg len == 0, means no more to send, just break the loop
    if ( msg.sequence_length() == 0 ) {
      break;
    }

    // update abs_seq
    _abs_seq += msg.sequence_length();

    // if the timer not started yet, start it.
    if ( !_retransmission_timer.isRunning() ) {
      _retransmission_timer.start();
    }

    // set RST
    msg.RST = has_error();

    // use transmit to send
    transmit( msg );

    // add msg to the outstanding_segments_collection
    _outstanding_segments_collection.push_back( std::move( msg ) );
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( _abs_seq, isn_ );
  // set RST
  msg.RST = has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // set the window size, if the msg.window_size == 0, set to 1.
  _receiver_window_size = msg.window_size;
  _window_size = _receiver_window_size == 0 ? 1 : _receiver_window_size;

  // check RST
  if ( msg.RST ) {
    set_error();
  }

  // When the receiver gives the sender an ackno that acknowledges the successful receipt
  // of new data (the ackno reflects an absolute sequence number bigger than any previous
  // ackno):
  if ( msg.ackno.has_value() ) {
    uint64_t abs_seq_ackno = msg.ackno.value().unwrap( isn_, _abs_seq );
    // if not ack a new data, just ignore so it won't reset the timer.
    // if Impossible ackno (beyond next seqno), also ignore.
    if ( abs_seq_ackno <= _pre_ack_ackno || abs_seq_ackno > _abs_seq ) {
      return;
    }

    _pre_ack_ackno = abs_seq_ackno;

    // remove segment in outstanding collections that all the sequence number <= _edge_left.
    for ( auto it = _outstanding_segments_collection.begin(); it != _outstanding_segments_collection.end(); ) {
      auto& sequence = *it;
      if ( sequence.seqno.unwrap( isn_, _abs_seq ) + sequence.sequence_length() <= abs_seq_ackno ) {
        _outstanding_sequence_numbers -= sequence.sequence_length();
        it = _outstanding_segments_collection.erase( it );
      } else {
        break;
      }
    }

    // Set the RTO back to its “initial value.”
    _RTO = initial_RTO_ms_;
    _retransmission_timer = timer( _RTO );
    // If the sender has any outstanding data, restart the retransmission timer so that it will expire after RTO
    // milliseconds
    if ( !_outstanding_segments_collection.empty() ) {
      _retransmission_timer.start();
    }
    // Reset the count of “consecutive retransmissions” back to zero.
    _consecutive_retransmissions_times = 0;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // if current has a timer running
  if ( _retransmission_timer.isRunning() ) {
    // reduce its RTO by ms_since_last_tick
    _retransmission_timer.reduce( ms_since_last_tick );

    // if RTO has expired
    if ( _retransmission_timer.RTO() <= 0 ) {
      // need  Retransmit the earliest (lowest sequence number) segment that hasn’t been fully acknowledged
      transmit( _outstanding_segments_collection.front() );
      // If the window size is nonzero:
      if ( _receiver_window_size > 0 ) {
        // increase consecutive retransmissions times
        _consecutive_retransmissions_times++;
        // Double the value of RTO.
        _RTO *= 2;
      }
      // reset timer.
      _retransmission_timer.reset( _RTO );
    }
  }
}
