#ifndef PAYLOAD_HH
#define PAYLOAD_HH

#include <stdint.h>
#include <string>

#include "util.hh"

class Packet {

public:
	
	class Header {
		
	public:

		uint64_t sendingtime; // 8
		uint32_t sequencenum; // 4
		uint16_t framenumber; // 2
		uint8_t  packetindex; // 1
		uint8_t  numpackets; // 1, in this frame
		uint32_t sentsofar; // 4

		Header( uint8_t n, uint16_t f ) :
			sendingtime( 0 ),
			sequencenum( 0 ),
			framenumber( f ),
			packetindex( 0 ),
			numpackets ( n )
		{}

		std::string GetHeader( uint32_t _s_, uint8_t _ix_, uint32_t _sofar_ ) {		
			sequencenum = _s_;
			packetindex = _ix_;
			sendingtime = GetTimeNow();
			sentsofar = _sofar_;

		return std::string( (char*)this, sizeof(Header) );
		}
		
		std::string GetHeader( void ) {
			return std::string( (char*)this, sizeof(Header) );
		}
		
	};
	
	class FbHeader {
	
	public:	
	
		uint64_t measuredbyte; // 8
		uint64_t measuredtime; // 8
		uint64_t sendingtime;
		uint32_t sequencenum; // 4
		uint32_t bytes_rcvd; // 4
		uint32_t bytes_lost; // 4
		
		FbHeader( uint64_t _bytes, uint64_t _interarrival, uint32_t _seqnum, uint32_t _numrecbytes, uint32_t _lost_ )
		: measuredbyte( _bytes )
		, measuredtime( _interarrival )
		, sendingtime( GetTimeNow() )
		, sequencenum( _seqnum )
		, bytes_rcvd( _numrecbytes )
		, bytes_lost( _lost_ )
		{};
	};
	
public:

	Header header;	
	std::string appdata;
	
	Packet( const char* ptr, size_t size )
    : header ( *(Packet::Header*)ptr )
    , appdata( std::string( ptr+sizeof(Header), size-sizeof(Header) ) ) 
	{}
	
	bool operator < (const Packet & other ) const {
		return header.packetindex < other.header.packetindex;
	}

	bool operator > (const Packet & other ) const {
		return header.packetindex > other.header.packetindex;
	}

	bool operator == (const Packet & other ) const {
		return header.packetindex == other.header.packetindex;
	}
};
#endif