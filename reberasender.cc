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
#include "v4l2.hh"
#include "encoder.hh"
}

#define RBR_MIN_ENCODING_RATE 300
#define RBR_MAX_ENCODING_RATE 3000

/* Ctrl-C handler */
static volatile int b_ctrl_c = 0;

static void sigint_handler( int a )
{
    b_ctrl_c = 1;
}

int main ( int argc, char* argv[] )
{
	signal( SIGINT, sigint_handler );

	//char camdevice[] = "/dev/video0";

	FILE* pAck = fopen( "/home/eymen/Desktop/ack.txt", "wb" );

	uint64_t start = 0;
	int i_frame_count = 0;
	SDL_Event evt;

	v4l2capture vidcap( argv[1] );
	x264encoder encode( vidcap.get_width(), vidcap.get_height() );
	SenderState sender( argv[2], argv[3], NULL );

	/* start capturing */
	vidcap.set_picbuff( &encode );
	vidcap.start_capturing();

	/* we capture events (arrival of acks, feedback packets, or new frames) using select */
	Select &sel = Select::get_instance();
	sel.add_fd( sender.socket.get_sock() );
	sel.add_fd( sender.acksocket.get_sock() );
	sel.add_fd( vidcap.get_fd() );

	while ( !b_ctrl_c )
	{
		while( SDL_PollEvent( &evt ) ) // Returns 1 if there is a pending event or 0 if there are none.
		{
			switch( evt.type ) {
				case SDL_QUIT :
					b_ctrl_c = 1;
					goto EXIT;
			/*	case SDL_WINDOWEVENT :
					switch (evt.window.event) {
					case SDL_WINDOWEVENT_SHOWN:
						SDL_Log("Window %d shown", evt.window.windowID);
						break;
					case SDL_WINDOWEVENT_HIDDEN:
						SDL_Log("Window %d hidden", evt.window.windowID);
						break;
					case SDL_WINDOWEVENT_EXPOSED:
						SDL_Log("Window %d exposed", evt.window.windowID);
						break;
					case SDL_WINDOWEVENT_MOVED:
						SDL_Log("Window %d moved to %d,%d", evt.window.windowID, evt.window.data1, evt.window.data2);
						break;
					case SDL_WINDOWEVENT_RESIZED:
						SDL_Log("Window %d resized to %dx%d", evt.window.windowID, evt.window.data1, evt.window.data2);
						break;
					case SDL_WINDOWEVENT_SIZE_CHANGED:
						SDL_Log("Window %d size changed to %dx%d", evt.window.windowID, evt.window.data1, evt.window.data2);
						break;
					case SDL_WINDOWEVENT_MINIMIZED:
						SDL_Log("Window %d minimized", evt.window.windowID);
						break;
					case SDL_WINDOWEVENT_MAXIMIZED:
						SDL_Log("Window %d maximized", evt.window.windowID);
						break;
					case SDL_WINDOWEVENT_RESTORED:
						SDL_Log("Window %d restored", evt.window.windowID);
						break;
					case SDL_WINDOWEVENT_ENTER:
						SDL_Log("Mouse entered window %d", evt.window.windowID);
						break;
					case SDL_WINDOWEVENT_LEAVE:
						SDL_Log("Mouse left window %d", evt.window.windowID);
						break;
					case SDL_WINDOWEVENT_FOCUS_GAINED:
						SDL_Log("Window %d gained keyboard focus", evt.window.windowID);
						break;
					case SDL_WINDOWEVENT_FOCUS_LOST:
						SDL_Log("Window %d lost keyboard focus", evt.window.windowID);
						break;
					case SDL_WINDOWEVENT_CLOSE:
						SDL_Log("Window %d closed", evt.window.windowID);
						break;
					default:
						SDL_Log("Window %d got unknown event %d", evt.window.windowID, evt.window.event);
						break;
					}
					break; */
				default :
					break;
			}
		}

		if ( !start ) start = GetTimeNow();

		if ( !b_ctrl_c ) sel.select( -1 );

		if ( sel.read( sender.socket.get_sock() ) ) /* incoming feedback packet awaiting */
		{
			Socket::Packet incoming( sender.socket.recv() );
			Packet::FbHeader* h = (Packet::FbHeader*)incoming.payload.data();
			sender.set_fb_time( GetTimeNow() );

			if ( h->measuredtime > 100 || ( h->measuredbyte == 0 && h->measuredtime == 0 ) )
			{
				sender.set_valid_time();
				sender.set_B( static_cast<double>(h->measuredbyte) ); // in bytes
				sender.set_T( static_cast<double>(h->measuredtime) ); // in microseconds
			}
			else printf( "!! T = %lu, B = %lu !!\n", h->measuredtime, h->measuredbyte );

			if ( (uint32_t)(h->bytes_rcvd) > sender.get_bytes_recv() )
				sender.set_bytes_recv( (uint32_t)(h->bytes_rcvd) );
			sender.set_bytes_lost( (uint32_t)(h->bytes_lost) );
		}
		else if ( sel.read( sender.acksocket.get_sock() ) ) /* incoming ack awaiting */
		{
			Socket::Packet incoming( sender.acksocket.recv() );
			Packet::Header* pHeader = (Packet::Header*)incoming.payload.data();

			/* log acks to measure 1-way delays */
			fprintf( pAck, "%lu %u %u\n", GetTimeNow()-pHeader->sendingtime,
										  pHeader->sequencenum,
										  pHeader->framenumber );
		}
		else /* new frame awaiting */
		{
			/* get the yuv picture and copy that to encoder's pic_in */
			if ( vidcap.read_frame() < 0 ) {
				fprintf( stderr, "> Rebera error: unable to read raw picture\n" );
				b_ctrl_c = 1;
			}

			int rate_cap = 0, budget = 0;
			unsigned i_poc = i_frame_count % RBR_INTRAPERIOD;

			if ( i_poc == 0 ) // IDR frame: time to make a prediction
			{
				budget = sender.predict_next();
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
		}
	}
EXIT:
	vidcap.stop_capturing();
	fclose( pAck );

return 0;
}
