#include "senderstate.hh"

SenderState::SenderState( char const* _remote_IP_, char const* _video_if_, const char* _ack_if_ )
: abs_start_time( GetTimeNow() )
, remote_addr( _remote_IP_, 42000 )
, rate_adapter( RBR_GOP, RBR_INTRAPERIOD, NULL )
, oracle( 5, 1 )
{
	if ( _video_if_ )
		socket.bind_to_device( _video_if_ );
	
	socket.bind( Socket::Address( "0.0.0.0", 42001 ) );
	
	if ( _ack_if_ ) { /* (experimental) if we want acks */
		acksocket.bind_to_device( _ack_if_ );
		acksocket.bind( Socket::Address( "0.0.0.0", 42003 ) );
	}
	
	i_budget = (int)(1e3 * RBR_INIT_PRED * d_ipr);
};

SenderState::~SenderState( void )
{
	//if ( pDfs != NULL ) fclose( pDfs );
	//if ( pBwLogFile != NULL ) fclose( pBwLogFile );
};

void SenderState::predict_next( void )
{
	if ( ul_fback_ts && 1.0*(GetTimeNow() - ul_fback_ts)/1e6 <= d_ipr )
	{
		double this_one = ( B == 0 && T == 0 ) ? 0 : 1000*B/T;
		
		d_measurement = ( T > RBR_GOOD_MEAS_THRE ) ? this_one : 0.5*this_one+0.5*d_measurement;
		
		d_margin = oracle.running_percentile( d_measurement, d_predicted );
		
		d_predicted = oracle.update( d_measurement );
		
		set_inqueue();

		i_budget = 1000 * d_ipr * (d_predicted * d_margin) - i_inqueue;
		i_budget = i_budget > 0 ? i_budget : 0;
	} 
	
	printf( "intra-period %d\tinput=%.0f\tpred=%.0f\tsafe=%.0f%%\tinqueue=%d\tbudget=%d\n",
		i_count, 8*d_measurement, 8*d_predicted, 100*d_margin, i_inqueue, i_budget );
	
	i_count++;
};

/* prevent deadlock when packets are thought to be in-queue but in fact they are lost */
void SenderState::set_inqueue( void ) {
	
	int inq = bytes_sent - bytes_rcvd - bytes_lost;
	
	i_wndw = ( i_inqueue == inq ) ? (i_wndw+1) : 0;
	
	if ( i_wndw == RBR_INQUEUE_WNDW ) {
		bytes_lost += i_inqueue;
		i_inqueue = 0;
	} else {
		i_inqueue = inq;
	}
}

void SenderState::init_rate_adapter( int _budget_in_bytes_ )
{
	rate_adapter.set_bw_budget( _budget_in_bytes_ );
	rate_adapter.refresh_order();
};

void SenderState::send_frame( const char* buff, size_t u_framesize, int i_enc_frame )
{
	Packet::Header header( (uint8_t)(u_framesize/u_maxpayload + 1), i_enc_frame );

	int sayz;

	for ( uint8_t i = 0; i <= header.numpackets; i++ ) {
		if ( i == 0 ) {
			bytes_sent += u_fullheader;
			sayz = 0;
			socket.send( Socket::Packet( remote_addr, header.GetHeader( seq_num++, i, bytes_sent ) ) );
		} else if ( i == header.numpackets ) {
			sayz = u_framesize%u_maxpayload;
			bytes_sent += sayz + u_fullheader;
			socket.send( Socket::Packet( remote_addr, header.GetHeader( seq_num++, i, bytes_sent ) + std::string( buff, sayz ) ) );
		} else {
			sayz = u_maxpayload;
			bytes_sent += sayz + u_fullheader;
			socket.send( Socket::Packet( remote_addr, header.GetHeader( seq_num++, i, bytes_sent ) + std::string( buff, sayz ) ) );
			buff += sayz;
		}
	}
};

void SenderState::send_frame_cellsim( const char* buff, size_t u_framesize, int _framenum_ )
{
 Packet::Header header( static_cast<uint8_t>(u_framesize/u_maxpayload), _framenum_ );

 /* send the dwarf packet first */
 bytes_sent += (u_framesize%u_maxpayload)+u_fullheader;
 socket.send( Socket::Packet( remote_addr, header.GetHeader( seq_num++, 0, bytes_sent )+std::string( buff, u_framesize%u_maxpayload ) ) );

 //fprintf( pPacketLogFile, "%lu\t%u\t%u\t%d\n", GetTimeNow(), header.sequencenum, (unsigned)(u_framesize%u_maxpayload + u_fullheader), header.framenumber );

 if ( header.numpackets )
 {
	 buff += u_framesize%u_maxpayload;
	 for ( uint8_t i = 1; i <= header.numpackets; i++ )
	 {
		 bytes_sent += u_maxpayload+u_fullheader;
		 socket.send( Socket::Packet( remote_addr, header.GetHeader( seq_num++, i, bytes_sent ) + std::string( buff, u_maxpayload ) ) );
		 buff += u_maxpayload;
		 //fprintf( pPacketLogFile, "%lu\t%u\t%u\t%d\n", GetTimeNow(), header.sequencenum, u_maxpayload + u_fullheader, header.framenumber );
	 }
 }
};


int SenderState::total_length( int _size_ ) {
	_size_ += ((unsigned)(_size_/u_maxpayload)+1)*u_fullheader;
	//~ _size_ += u_fullheader;
return _size_;
};
