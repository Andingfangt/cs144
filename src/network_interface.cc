#include <iostream>
#include <type_traits>

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "network_interface.hh"
#include "parser.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  , datagrams_received_()
  , IPMappings_()
  , ARPRequest_()
  , datagrams_queue_()
  , IPmappingTimeout_( 30000 ) // 30s
  , ARPRequestTimeout_( 5000 ) // 5s

{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  auto raw_32_ip_adress = next_hop.ipv4_numeric();
  // If the destination Ethernet address is already known, send it right away.
  if ( IPMappings_.contains( raw_32_ip_adress ) ) {
    // Create an Ethernet frame (with type = EthernetHeader::TYPE IPv4),
    EthernetFrame frame {};
    frame.header.type = EthernetHeader::TYPE_IPv4;
    // set the payload to be the serialized datagram
    frame.payload = serialize( dgram );
    // set the source and destination addresses.
    frame.header.src = ethernet_address_;
    frame.header.dst = IPMappings_[raw_32_ip_adress].first;

    // send the frame using transmit();
    transmit( frame );
  }
  // If the destination Ethernet address is unknown
  else {
    // queue the IP datagram so it can be sent after the ARP reply is received.
    // this should be add before send the ARP request frame.
    datagrams_queue_.insert( { raw_32_ip_adress, dgram } );

    // check if have send ARP request for this IP address and whether it is dead.
    if ( !ARPRequest_.contains( raw_32_ip_adress ) || ARPRequest_[raw_32_ip_adress] > ARPRequestTimeout_ ) {
      // broadcast an ARP request for the next hop’s Ethernet address
      ARPMessage ARPrequst {};
      ARPrequst.opcode = ARPMessage::OPCODE_REQUEST;
      ARPrequst.sender_ethernet_address = ethernet_address_;
      ARPrequst.sender_ip_address = ip_address_.ipv4_numeric();
      ARPrequst.target_ip_address = raw_32_ip_adress;

      // Packaging APR requests into Ethernet frames
      EthernetFrame frame {};
      frame.header.type = EthernetHeader::TYPE_ARP;
      frame.header.src = ethernet_address_;
      frame.header.dst = ETHERNET_BROADCAST;
      frame.payload = serialize( ARPrequst );

      // Send the ARP request frame
      transmit( frame );

      // Record the time when the ARP request was sent.
      ARPRequest_[raw_32_ip_adress] = 0;
    }
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // check the target address to see if it is send to us.
  if ( frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_ ) {
    return;
  }

  // If the inbound frame is IPv4
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    // parse the payload as an InternetDatagram
    InternetDatagram dgram {};
    // if successful push the resulting datagram on to the datagrams received queue.
    if ( parse( dgram, frame.payload ) ) {
      datagrams_received_.push( std::move( dgram ) );
    }
  }
  // If the inbound frame is ARP
  else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    // parse the payload as an ARPMessage
    ARPMessage ARP_dgram {};
    // if successful
    if ( parse( ARP_dgram, frame.payload ) ) {
      // first check if the dst ip address is us
      if ( ARP_dgram.target_ip_address != ip_address_.ipv4_numeric() ) {
        return;
      }
      // remember the mapping between the sender’s IP address and Ethernet address for 30 seconds.
      IPMappings_[ARP_dgram.sender_ip_address] = { ARP_dgram.sender_ethernet_address, 0 };
      // send all the waiting messages in datagram_queue that key = ARP_dgram.sender_ip_address
      auto watting_dgrams = datagrams_queue_.equal_range( ARP_dgram.sender_ip_address );
      for ( auto it = watting_dgrams.first; it != watting_dgrams.second; ) {
        send_datagram( it->second, Address::from_ipv4_numeric( ARP_dgram.sender_ip_address ) );
        it = datagrams_queue_.erase( it );
      }

      // if it’s an ARP request asking for our IP address, send an appropriate ARP reply.
      if ( ARP_dgram.opcode == ARPMessage::OPCODE_REQUEST ) {
        ARPMessage ARPReply {};
        ARPReply.opcode = ARPMessage::OPCODE_REPLY;
        ARPReply.sender_ethernet_address = ethernet_address_;
        ARPReply.sender_ip_address = ip_address_.ipv4_numeric();
        ARPReply.target_ip_address = ARP_dgram.sender_ip_address;
        ARPReply.target_ethernet_address = ARP_dgram.sender_ethernet_address;

        EthernetFrame send_frame {};
        send_frame.header.type = EthernetHeader::TYPE_ARP;
        send_frame.header.src = ethernet_address_;
        send_frame.header.dst = ARP_dgram.sender_ethernet_address;
        send_frame.payload = serialize( ARPReply );

        transmit( send_frame );
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // for all mapping in IPmappings, add time and remove all the dead mappings
  for ( auto it = IPMappings_.begin(); it != IPMappings_.end(); ) {
    it->second.second += ms_since_last_tick;
    if ( it->second.second >= IPmappingTimeout_ ) {
      it = IPMappings_.erase( it );
    } else {
      it++;
    }
  }

  // for all ARP requst, add time and remove all the dead requst
  for ( auto it = ARPRequest_.begin(); it != ARPRequest_.end(); ) {
    it->second += ms_since_last_tick;
    if ( it->second >= ARPRequestTimeout_ ) {
      it = ARPRequest_.erase( it );
    } else {
      it++;
    }
  }
}
