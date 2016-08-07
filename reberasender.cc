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

/* Ctrl-C handler */
static volatile int b_ctrl_c = 0;

static void sigint_handler( int a )
{
    b_ctrl_c = 1;
}

int main ( int argc, char* argv[] )
{
	if ( argc != 3 ) {
		printf("Usage: %s [REMOTE_IPv4] [INTERFACE]", argv[0]);
		return -1;
	}
	
	signal( SIGINT, sigint_handler );
	
	char device[] = "/dev/video1"; // TODO: present options to the user
	v4l2capture vidcap( device );
	
	ReberaEncoder encoder( vidcap.get_width(), vidcap.get_height() );
	encoder.attach_camera( &vidcap );
	encoder.reduce_fr_by( 1 );
	
	SenderState sender( argv[1], argv[2], NULL );
	sender.set_ipr( encoder.get_ipr() );

	/* start capturing */
	vidcap.set_picbuff( encoder.get_Ybuff(), encoder.get_Cbbuff(), encoder.get_Crbuff() );
	vidcap.start_capturing();

	/* we catch events (arrival of acks, feedback packets, or new frames) using select */
	Select &sel = Select::get_instance();
	sel.add_fd( sender.socket.get_sock() );
	sel.add_fd( sender.acksocket.get_sock() );
	sel.add_fd( vidcap.get_fd() );

	while ( !b_ctrl_c )
	{
		SDL_Event evt;
		while( SDL_PollEvent( &evt ) ) // Returns 1 if there is a pending event or 0 if there are none.
		{
			switch( evt.type ) {
				case SDL_QUIT :
					b_ctrl_c = 1;
					goto EXIT;
				default :
					break;
			}
		}

		if ( !b_ctrl_c ) sel.select( -1 );

		if ( sel.read( sender.socket.get_sock() ) ) /* incoming feedback packet awaiting */
		{
			Socket::Packet incoming( sender.socket.recv() );
			Packet::FbHeader* h = (Packet::FbHeader*)incoming.payload.data();

			if ( h->measuredtime > 100 || ( h->measuredbyte == 0 && h->measuredtime == 0 ) )
			{
				sender.set_fb_time( GetTimeNow() );
				sender.set_B( static_cast<double>(h->measuredbyte) ); // in bytes
				sender.set_T( static_cast<double>(h->measuredtime) ); // in microseconds
			}

			if ( (uint32_t)(h->bytes_rcvd) > sender.get_bytes_recv() )
				sender.set_bytes_recv( (uint32_t)(h->bytes_rcvd) );
			sender.set_bytes_lost( (uint32_t)(h->bytes_lost) );
		}
		else if ( sel.read( sender.acksocket.get_sock() ) ) /* incoming ack awaiting */
		{
			Socket::Packet incoming( sender.acksocket.recv() );
			//Packet::Header* pHeader = (Packet::Header*)incoming.payload.data();
		}
		else /* new raw picture awaiting */
		{
			/* get the next eligible raw picture and copy to encoder's pic_in */
			if ( encoder.get_frame() )
			{
				if ( !encoder.get_frame_index() ) // IDR frame: time to make a prediction
				{
					sender.predict_next();
					encoder.change_bitrate( 0.008*sender.get_budget() / encoder.get_ipr() );
				}

				/* encode the picture */
				int i_frame_size = encoder.compress_frame();
				
				int i_total_size = sender.total_length( i_frame_size );

				if ( !encoder.get_frame_index() )
					sender.init_rate_adapter( sender.get_budget() < i_total_size && sender.get_inqueue() == 0 ? i_total_size : sender.get_budget() );
					
				if ( sender.rate_adapter.permits( i_total_size, encoder.get_frame_index() ) )
				{	
					std::string frame( (const char*)encoder.get_nal(), (size_t)i_frame_size );
					
					sender.send_frame( frame.data(), i_frame_size, encoder.i_enc_frame );
				}
				encoder.i_enc_frame++;
			}
		}
	}
EXIT:
	vidcap.stop_capturing();

return 0;
}
