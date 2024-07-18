#include "socket.hh"

#include "tcp_minnow_socket.hh"
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <sys/socket.h>

using namespace std;

void get_URL( const string& host, const string& path )
{
  // cerr << "Function called: get_URL(" << host << ", " << path << ")\n";
  // cerr << "Warning: get_URL() has not been implemented yet.\n";

  // TCPSocket tcp;                   // default construct for TCPSocket
  CS144TCPSocket tcp; // default constructor for TCPSocket

  Address address( host, "http" ); // Construct by resolving a hostname and servicename.
  // Connect a socket to a specified peer address
  tcp.connect( address );
  string line1 = "GET " + path + " HTTP/1.1" + "\r\n"; // This tells the server the path part of the URL.
  string line2 = "Host: " + host + "\r\n";             // This tells the server the host part of the URL.
  string line3 = "Connection: close\r\n"; // This tells the server that you are finished making requests, and it
                                          // should close the connection as soon as it finishes replying.
  string line4 = "\r\n"; // Hit the Enter key one more time. This sends an empty line and tells the server that you
                         // are done with your HTTP request.
  tcp.write( line1 + line2 + line3 + line4 );

  // print all the output from the server until the socket reaches “EOF”
  string buffer;
  while ( !tcp.eof() ) {
    string part;
    tcp.read( part );
    buffer += part;
  }
  cout << buffer;
  // tcp.close(); // this close for TCPSocket
  tcp.wait_until_closed(); // this close for CS144TCPSocket
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
