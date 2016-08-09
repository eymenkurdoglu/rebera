#ifndef NETWORKSTATE_HH
#define NETWORKSTATE_HH

#include <set>
#include <list>

#include "payload.hh"
#include "videostate.hh"
#include "socket.hh"

extern "C"
{
#include <libavcodec/avcodec.h>
}

#define RBR_NETWORK_STACK_HEADER 42
#define RBR_FB_PER_IPER 16

struct ReceivedPacket {

	Packet* pcktPtr; // pointer to the network packet (video data + header)
	uint64_t rcv_ts; // time at which this network packet was received

	bool operator < (const ReceivedPacket & other ) const {
		return pcktPtr->header.packetindex < other.pcktPtr->header.packetindex;
	}

	bool operator > (const ReceivedPacket & other ) const {
		return pcktPtr->header.packetindex > other.pcktPtr->header.packetindex;
	}

	bool operator == (const ReceivedPacket & other ) const {
		return pcktPtr->header.packetindex == other.pcktPtr->header.packetindex;
	}

	ReceivedPacket( Packet* _packet_ptr, uint64_t _receive_ts ) :
		pcktPtr( _packet_ptr ), rcv_ts( _receive_ts )
	{};
};

struct PartialFrame
{
	std::set<ReceivedPacket> packet_set;

    int		 i_num_packets;
    int 	 i_frame_size;
    uint32_t u_frame_num;
    uint64_t lastarrival_ts;

    PartialFrame( Packet* p, uint64_t ts )
    : i_num_packets( 1 + p->header.numpackets )
    , i_frame_size( p->appdata.size() )
    , u_frame_num( p->header.framenumber )
    , lastarrival_ts( ts )
    {
		packet_set.insert( ReceivedPacket( p, ts ) );
	}
};

class ReberaReceiver {

private:

	uint64_t m_measuredbyte, m_measuredtime, m_streamstart;
	uint64_t B[RBR_FB_PER_IPER], T[RBR_FB_PER_IPER];

	ReberaDecoder* m_pVideoState;

	Socket::Address m_remote_addr;

	bool m_connected =false;
	bool m_stream_alive = false;
	bool m_ack = false;

public:

	std::list<PartialFrame> incomplete;

    uint32_t bytes_received = 0;
    uint32_t bytes_lost = 0;
    uint32_t fb_seq_no = 0;

	Socket sink_sock;
	Socket ack_sock;

public:

	ReberaReceiver( ReberaDecoder*, const char* );

	void set_remote_addr( const Socket::Address & _remote_ ) { m_remote_addr = _remote_; };
	void set_measured_byte( uint64_t b ) { m_measuredbyte = b; };
	void set_measured_time( uint64_t t ) { m_measuredtime = t; };
	void set_connected( void ) { m_connected = true; };
	void set_stream_start_ts( uint64_t t ) { m_streamstart = t; };
	void stream_is_alive( void ) { m_stream_alive = true; };
	void stream_is_dead( void ) { m_stream_alive = false; };
	void slides_windows( void );

	Packet::FbHeader get_fb_header( void )
	{
		uint64_t sumB = 0, sumT = 0;
		for ( int i = 0; i < RBR_FB_PER_IPER; i++ ) { sumB += B[i]; sumT += T[i]; }
		m_measuredbyte = 0;
		m_measuredtime = 0;
		return Packet::FbHeader( sumB, sumT, ++fb_seq_no, bytes_received, bytes_lost );
	};
	Socket::Address get_remote_addr( void ) const { return m_remote_addr; };
	ReberaDecoder* get_videostate( void ) const { return m_pVideoState; };
	uint64_t 	get_measured_byte( void ) const { return m_measuredbyte; };
	uint64_t 	get_measured_time( void ) const { return m_measuredtime; };
	uint64_t	get_stream_start_ts( void ) const { return m_streamstart; };

	bool got_new_frame( Packet*, uint64_t );
	AVPacket* check_and_queue( void );
	void place_in_buffer( Packet*, uint64_t );

	bool is_connected( void ) { return m_connected; };
	bool is_stream_alive( void ) { return m_stream_alive; };

	AVPacket* create_AVPacket( std::list<PartialFrame>::iterator );
	void pass_to_decoder_queue( std::list<PartialFrame>::iterator );
};
#endif
