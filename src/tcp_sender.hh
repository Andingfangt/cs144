#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>

class timer
{
public:
  /* default constructe*/
  timer( uint64_t RTO ) : _is_running( false ), _RTO( RTO ) {}

  /* start the timer*/
  void start() { _is_running = true; }

  /* stop the timer*/
  void stop() { _is_running = false; }

  /* determing if this timer is running*/
  bool isRunning() const { return _is_running; }

  /* reduce the timer's RTO with passed time*/
  void reduce( uint64_t passed_time ) { _RTO -= passed_time; }

  /* get the current timer's RTO*/
  int RTO() const { return _RTO; }

  /* reset the timer's RTO to the given RTO, used when timeout.*/
  void reset( int RTO ) { _RTO = RTO; }

private:
  bool _is_running; // true if the timer is running.
  int _RTO;         // current RTO value, use for Retransmission Timeout.
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) )
    , isn_( isn )
    , _abs_seq( 0 )
    , initial_RTO_ms_( initial_RTO_ms )
    , _RTO( initial_RTO_ms )
    , _outstanding_sequence_numbers( 0 )
    , _outstanding_segments_collection()
    , _consecutive_retransmissions_times( 0 )
    , _receiver_window_size( 1 ) // initialize to 1, so the first check for nonezero window size will pass
    , _window_size( 1 )          // at lest 1.
    , _retransmission_timer( initial_RTO_ms )
    , _has_send_SYN( false )
    , _has_send_FIN( false )
    , _pre_ack_ackno( 0 )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

  bool has_error() const { return input_.has_error(); }
  void set_error() { input_.set_error(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;       // the zero_point.
  uint64_t _abs_seq; // the checkpoint
  uint64_t initial_RTO_ms_;
  uint64_t _RTO;                          // use for exponetial backoff.
  uint64_t _outstanding_sequence_numbers; // use for count how many sequence numbers are outstanding
  std::deque<TCPSenderMessage> _outstanding_segments_collection; // store all the outstanding messages
  uint64_t _consecutive_retransmissions_times; // use for count how many consecutive *re*transmissions have
                                               // happened, use for exponential backoff
  uint16_t _receiver_window_size;              // Receiver's window size
  uint16_t _window_size;                       // Appearance window size
  timer _retransmission_timer;                 // true if the timer is running.
  bool _has_send_SYN;                          // detemine if have send SYN
  bool _has_send_FIN;                          // detemine if have send FIN
  uint64_t _pre_ack_ackno;                     // the biggest previous ACK ackno.
};
