#include "receiverstate.hh"

ReberaReceiver::ReberaReceiver( ReberaVideo* _video_state, const char* _ack_if_ )
: m_measuredbyte( 0 )
, m_measuredtime( 0 )
, m_pVideoState( _video_state )
, m_remote_addr( std::string("0.0.0.0"), 0 )
, m_connected( false )
{
	for ( int i = 0; i < RBR_FB_PER_IPER; i++ )
	{
		B[i]=0; T[i]=0;
	}
	sink_sock.bind( Socket::Address ( "0.0.0.0", 42000 ) );
	if ( _ack_if_ ) {
		ack_sock.bind_to_device( _ack_if_ );
		ack_sock.bind( Socket::Address( "0.0.0.0", 42002 ) );
	}
}

void ReberaReceiver::slides_windows( void )
{
	for ( int i = RBR_FB_PER_IPER-2; i >= 0; --i) {
		B[i+1] = B[i]; T[i+1] = T[i];
	}
	B[0] = m_measuredbyte;
	T[0] = m_measuredtime;
};

/*
creates an "AVPacket" object to store the compressed frame
returns its pointer
*/
AVPacket* ReberaReceiver::create_AVPacket( std::list<PartialFrame>::iterator it )
{
	/* allocate memory for "AVPacket" and initialize */
	AVPacket* av_packet = (AVPacket*)av_malloc( sizeof(AVPacket) ); // FREE LATER!!!
	av_new_packet( av_packet, it->i_frame_size );
	av_packet->size = it->i_frame_size;

	/* copy into "AVPacket" from received packets */
	uint8_t* pointer = av_packet->data;
	for ( auto itt = it->packet_set.begin(); itt != it->packet_set.end(); itt++ )
	{
		memcpy( pointer, itt->pcktPtr->appdata.data(), itt->pcktPtr->appdata.size() );
		pointer += itt->pcktPtr->appdata.size();
	}
return av_packet;
}

/*
checks each frame fragment
if a complete frame is found, puts it in the decoder queue
*/
AVPacket* ReberaReceiver::check_and_queue()
{
	for ( auto it = incomplete.begin(); it != incomplete.end(); it++ )
	{
		/* see if the frame is complete */
		if ( it->packet_set.size() == (unsigned)it->i_num_packets )
		{
			AVPacket* _avpacket = create_AVPacket( it );
			//m_pVideoState->GetAVPacketQueue()->PutAVPacket( create_AVPacket( it ) );
			//get_videostate()->decode_frame(  );
			//get_videostate()->refresh_display();
			for ( auto itt = it->packet_set.begin(); itt != it->packet_set.end(); itt++ )
				delete itt->pcktPtr;
			incomplete.erase( it );
			return _avpacket;
		}
	}
return NULL;
}

void ReberaReceiver::place_in_buffer( Packet* new_packet, uint64_t ts )
{
	/* see if new_packet belongs to an incomplete frame */
	for ( auto it = incomplete.begin(); it != incomplete.end(); it++ )
		if ( it->u_frame_num == new_packet->header.framenumber )
		{
			m_measuredbyte += 66 + new_packet->appdata.size();
			m_measuredtime += ts - it->lastarrival_ts;

			it->lastarrival_ts = ts;
			it->i_frame_size += new_packet->appdata.size();
			it->packet_set.insert( ReceivedPacket( new_packet, ts ) );
			return;
		}

	incomplete.push_back( PartialFrame( new_packet, ts ) );
}
