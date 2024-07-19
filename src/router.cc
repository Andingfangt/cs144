#include "router.hh"
#include "address.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  _routing_table.insert( { { prefix_length, route_prefix }, { next_hop, interface_num } } );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // loop all the interfaces and process all datagrams stored in the buffer queue of the network interface
  for ( auto& interface : _interfaces ) {
    auto& datagrams_queue = interface->datagrams_received();
    while ( !datagrams_queue.empty() ) {
      auto datagram = datagrams_queue.front();
      datagrams_queue.pop();
      // The router decrements the datagram’s TTL (time to live). If the TTL was zero already,
      // or hits zero after the decrement, the router should drop the datagram.
      if ( datagram.header.ttl == 0 || ( --datagram.header.ttl ) == 0 ) {
        continue;
      }
      // reset the checksum
      datagram.header.compute_checksum();

      // searches the routing table to find the routes that
      // match the datagram’s destination address.
      for ( auto& route : _routing_table ) {
        const auto& prefix_length = route.first.first;
        const auto& route_prefix = route.first.second;
        const auto& next_hop = route.second.first;
        const auto& interface_num = route.second.second;

        // if find a max prefix match, sends the modified datagram on the appropriate interface (
        // interface(interface num)->send datagram() ) to the appropriate next hop.
        if ( prefix_length == 0
             || ( datagram.header.dst >> ( 32 - prefix_length ) ) == ( route_prefix >> ( 32 - prefix_length ) ) ) {
          // if the next hop is an empty optional, the next hop is the datagram’s destination address.
          if ( !next_hop.has_value() ) {
            this->interface( interface_num )
              ->send_datagram( datagram, Address::from_ipv4_numeric( datagram.header.dst ) );
          }
          // else the next hop will contain the IP address of the next router along the path.
          else {
            this->interface( interface_num )->send_datagram( datagram, next_hop.value() );
          }
          break; // if find a match, no need to search other routes.
        }
      }
    }
  }
}
