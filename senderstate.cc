#include "senderstate.hh"

SenderState::SenderState( const char* _remote_IP_, const char* _video_if_, const char* _ack_if_ )
: abs_start_time( GetTimeNow() )
, remote_addr( _remote_IP_, 42000 )
//, pDfs( fopen( "/home/eymen/Desktop/dfs.txt", "wb" ) )
, pDfs( NULL )
, pBwLogFile( fopen( "/home/eymen/Desktop/bw.txt", "wb" ) )
, pPacketLogFile( fopen( "/home/eymen/Desktop/sender_packets.txt", "wb" ) )
, rate_adapter( RBR_GOP, RBR_INTRAPERIOD, pDfs )
, oracle( 5, 1 )
{
	socket.bind_to_device( _video_if_ );
	socket.bind( Socket::Address( "0.0.0.0", 42001 ) );
	if ( _ack_if_ ) { /* (experimental) if we want acks */
		acksocket.bind_to_device( _ack_if_ );
		acksocket.bind( Socket::Address( "0.0.0.0", 42003 ) );
	}
};

SenderState::~SenderState( void )
{
	if ( pDfs != NULL ) fclose( pDfs );
	if ( pBwLogFile != NULL ) fclose( pBwLogFile );
};

int SenderState::predict_next( void )
{
	uint64_t ago_valid = GetTimeNow() - last_valid_rec;

	double this_one = ( B == 0 && T == 0 ) ? 0 : 1000*B/T;
	d_measurement = ( T > RBR_GOOD_MEASUREMENT_THRESHOLD ) ? this_one : 0.5*this_one+0.5*d_measurement;
	if ( d_predicted ) d_margin = oracle.fiveperc( d_measurement/d_predicted );
	d_predicted = oracle.update( d_measurement );
	d_predicted = d_predicted > 0 ? d_predicted : 0;

	/* prevent deadlock when packets are thought to be in-queue but in fact they are lost */
	int inq = bytes_sent - bytes_rcvd - bytes_lost;
	i_wndw = ( i_inqueue == inq ) ? (i_wndw+1) : 0;
	if ( i_wndw == 4 ) { bytes_lost += i_inqueue; i_inqueue = 0;}
	else i_inqueue = inq;

	if ( ago_valid <= (uint64_t)(1000000*d_intraperiod) )
		i_budget = 1000*d_intraperiod*(d_predicted * ( d_margin<=1 ? d_margin : 1 ) ) - i_inqueue;

	printf( "%.2f\tinput=%.0f\tpred=%.0f\tsafe=%.0f%%\tinqueue=%d\tbudget=%d\n",
		(double)(GetTimeNow()-abs_start_time)/1000000, 8*d_measurement, 8*d_predicted, 100*d_margin, i_inqueue, i_budget );

	fprintf( pBwLogFile, "%.0f %.0f %.0f %.1f %d %d\n",
		8*this_one, 8*d_measurement, 8*d_predicted, 100*d_margin, i_inqueue, i_budget );

return i_budget > 0 ? i_budget : 0;
};

void SenderState::init_rate_adapter( int _budget_in_bytes_ )
{
	rate_adapter.set_bw_budget( _budget_in_bytes_ );
	rate_adapter.refresh_order();
};

void SenderState::send_frame( const char* buff, size_t u_framesize, int _framenum_ )
{
	uint8_t num_pack = (uint8_t)(u_framesize/u_maxpayload) + 1;
	Packet::Header header( num_pack, _framenum_ );

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
		fprintf( pPacketLogFile, "%lu\t%u\t%u\t%d\n", GetTimeNow(), header.sequencenum, sayz + u_fullheader, header.framenumber );
	}
};

int SenderState::total_length( int _size_ ) {
	_size_ += ((unsigned)(_size_/u_maxpayload)+1)*u_fullheader;
	//~ _size_ += u_fullheader;
return _size_;
};
