#pragma once

#include "reassembler.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

class TCPReceiver
{
public:
  // Construct with given Reassembler
  explicit TCPReceiver( Reassembler&& reassembler ) 
  : _reassembler( std::move( reassembler ) ) 
  , _zero_point( 0 )  // Initialize zero point to 0
  , SYN( false ) // Initialize SYN to false
  , RYN( false ) // Initialize RYN to false
  {}

  /*
   * The TCPReceiver receives TCPSenderMessages, inserting their payload into the Reassembler
   * at the correct stream index.
   */
  void receive( TCPSenderMessage message );

  // The TCPReceiver sends TCPReceiverMessages to the peer's TCPSender.
  TCPReceiverMessage send() const;

  // Access the output (only Reader is accessible non-const)
  const Reassembler& reassembler() const { return _reassembler; }
  Reader& reader() { return _reassembler.reader(); }
  const Reader& reader() const { return _reassembler.reader(); }
  const Writer& writer() const { return _reassembler.writer(); }

  // set error to streams
  void set_error() { _reassembler.set_error(); }
  // return true if has error in streams
  bool has_error() const { return _reassembler.has_error(); }


private:
  Reassembler _reassembler;
  Wrap32 _zero_point; // use for unwrap
  bool SYN; // if set, means we've received a SYN from the peer
  bool RYN; // if set, means error happend.
};
