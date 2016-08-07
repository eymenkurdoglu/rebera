#ifndef SENDERSTATE_HH
#define SENDERSTATE_HH

#include <cstring>
#include <stdio.h>
#include <assert.h>
#include <unistd.h> // for usleep
#include <cmath>
#include <signal.h>

#include "socket.hh"
#include "payload.hh"
#include "dfs.hh"
#include "util.hh"

#define RBR_NETWORK_STACK_HEADER 42 // ETHERNET(14)+IP(20)+UDP(8)
#define RBR_GOOD_MEAS_THRE 700000
#define RBR_INTRAPERIOD 32
#define RBR_INIT_PRED 12
#define RBR_MTU 1442
#define RBR_FPS 30
#define RBR_GOP 4
#define RBR_INQUEUE_WNDW 4

/*
* This class holds all the relevant information regarding a data stream sender.
* Works over UDP. Takes the size of the app header and the remote host IP as arguments.
*/
class SenderState
{
private:
	int 	 i_wndw = 0;

	uint16_t frame_num  = 0; // total number of frames generated so far
	uint32_t seq_num    = 0; // total number of packets sent so far
	uint32_t bytes_sent = 0; // total number of bytes sent so far
	uint32_t bytes_rcvd = 0; // total number of bytes received by the remote host so far
	uint32_t bytes_lost = 0; // total number of bytes written off as lost by the remote host so far

	const uint64_t abs_start_time; // start time
	uint64_t ul_fback_ts    = 0; // age of the last fb measurement

	const unsigned u_fullheader = RBR_NETWORK_STACK_HEADER + sizeof( Packet::Header );
	const unsigned u_maxpayload = RBR_MTU - u_fullheader;

	Socket::Address remote_addr;

	// FIXME add a congestion control object
	uint64_t T = 0;
	uint64_t B = 0;
	double 	d_measurement = 0;
	double 	d_predicted   = RBR_INIT_PRED; // in kilobytes per second
	double	d_margin	  = 1;  // 0 <= margin <= 1
	int		i_inqueue	  = 0;  // in bytes
	int 	i_budget;  // in bytes
	
	double d_ipr = 16.0/15;

public:

	Socket 	  socket;
	Socket	  acksocket;
	Dfs 	  rate_adapter;
	Predictor oracle;

	SenderState( const char*, const char*, const char* );
	~SenderState( void );

	void set_fb_time( uint64_t _time_ ) { ul_fback_ts = _time_; };
	void set_B( double _B_ ) { B = _B_; };
	void set_T( double _T_ ) { T = _T_; };
	void set_bytes_recv( uint32_t _recv_ ) { bytes_rcvd = _recv_; };
	void set_bytes_lost( uint32_t _lost_ ) { bytes_lost = _lost_; };
	void set_ipr( double _ipr_ ) { d_ipr = _ipr_; };

	void predict_next( void );
	void init_rate_adapter( int );
	int	 total_length( int );
	void send_frame( const char*, size_t, int );
	void set_inqueue( void );

	double 	get_B( void ) { return B; };
	double 	get_T( void ) { return T; };
	int	 	get_inqueue( void ) { return i_inqueue; };
	int	 	get_budget( void ) { return i_budget; };
	uint32_t get_bytes_recv( void ) { return bytes_rcvd; };
	Socket::Address get_remote_addr( void ) const { return remote_addr; };
};

#endif
