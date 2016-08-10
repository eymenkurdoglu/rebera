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
#include "select.h"
#include "senderstate.hh"

extern "C" {
#include "encoder.hh"
}

#define RBR_MIN_ENCODING_RATE 200
#define RBR_MAX_ENCODING_RATE 3000

/* Ctrl-C handler */
static volatile int b_ctrl_c = 0;

static void sigint_handler( int a )
{
    b_ctrl_c = 1;
}

int main ( int argc, char* argv[] )
{
	if ( !(argc == 3 || argc == 4) ) {
		printf( "> Rebera YUV sender usage: %s [REMOTE_IP] [INTERFACE] ([ACK_INTERFACE])\n", argv[0] );
		return 0;
	}

	signal( SIGINT, sigint_handler );

	FILE* pAck = fopen( "/home/eymen/Desktop/ack.txt", "wb" );

	uint64_t start = 0, next = 0, now = 0;
	int i_frame_count = 0, fps = 30;

	ReberaEncoder encode( 352, 288 );
	encode.attach_file( fopen( "CREW_352x288_30.yuv", "rb" ) );
	//ReberaEncoder encode( 704, 576, fopen( "CREW_704x576_30.yuv", "rb" ) );

	SenderState sender( argv[1], argv[2], argv[3] );

	Select &sel = Select::get_instance(); // watch for events using select
	sel.add_fd( sender.socket.get_sock() );
	sel.add_fd( sender.acksocket.get_sock() );

	while ( !b_ctrl_c )
	{
		now = GetTimeNow();
		if ( !start ) start = now;
		uint64_t waiting_time = start + next >= now ? start + next - now : 0;

		if ( !b_ctrl_c ) sel.select( waiting_time );

		if ( sel.read( sender.socket.get_sock() ) )
		{
			Socket::Packet incoming( sender.socket.recv() );
			Packet::FbHeader* pFbHeader = (Packet::FbHeader*)incoming.payload.data();
			sender.set_fb_time( GetTimeNow() );

			if ( pFbHeader->measuredtime > 10000 || ( pFbHeader->measuredbyte == 0 && pFbHeader->measuredtime == 0 ) )
			{
				sender.set_valid_time();
				sender.set_B( static_cast<double>(pFbHeader->measuredbyte) ); // in bytes
				sender.set_T( static_cast<double>(pFbHeader->measuredtime) ); // in microseconds
			}
			//else printf( "!! T = %lu, B = %lu !!\n", pFbHeader->measuredtime, pFbHeader->measuredbyte );

			if ( (uint32_t)(pFbHeader->bytes_rcvd) > sender.get_bytes_recv() )
				sender.set_bytes_recv( (uint32_t)(pFbHeader->bytes_rcvd) );
			sender.set_bytes_lost( (uint32_t)(pFbHeader->bytes_lost) );
		}
		else if ( sel.read( sender.acksocket.get_sock() ) )
		{
			Socket::Packet incoming( sender.acksocket.recv() );
			Packet::Header* pHeader = (Packet::Header*)incoming.payload.data();
			fprintf( pAck, "%lu %u %u\n", GetTimeNow()-pHeader->sendingtime, pHeader->sequencenum, pHeader->framenumber );
		}
		else
		{
			/* get the yuv picture from the input file and copy that to encoder's pic_in */
			if ( encode.read_frame() < 0 ) {
				fprintf( stderr, "> Rebera error: unable to read raw picture\n" );
				b_ctrl_c = 1;
			}

			int rate_cap = 0, budget = 0;
			unsigned i_poc = i_frame_count % RBR_INTRAPERIOD;

			if ( i_poc == 0 )  // IDR frame: time to predict AWB
			{
				budget = i_frame_count > 0 ? sender.predict_next() : 16000;
				rate_cap = 0.9*0.008*budget/(16/15);

				if ( rate_cap < RBR_MIN_ENCODING_RATE )
					rate_cap = RBR_MIN_ENCODING_RATE;
				else if ( rate_cap > RBR_MAX_ENCODING_RATE )
					rate_cap = RBR_MAX_ENCODING_RATE;

				if ( encode.change_bitrate( rate_cap ) < 0 )
					printf( "> Rebera error: x264 encoding bitrate could not be set\n" );
			}

			/* encode the picture */
			int i_frame_size = encode.encode_frame();
			int i_total_size = sender.total_length( i_frame_size );

			if ( i_poc == 0 ) {
				if ( sender.get_inqueue() == 0 )
					budget = budget < i_total_size ? i_total_size : budget;
				sender.init_rate_adapter( budget );
			}

			if ( sender.rate_adapter.permits( i_total_size, i_poc ) ) {
				std::string frame( (const char*)encode.get_nal(), (size_t)i_frame_size );
				sender.send_frame( frame.data(), i_frame_size, i_frame_count );
			}

			i_frame_count++;
			next = (1000000/fps)*i_frame_count;
		}
	}

sender.socket.send( Socket::Packet( sender.get_remote_addr(), std::string() ) );
fclose( pAck );
return 0;
}
