#include <cstring>
#include <stdio.h>
#include <assert.h>
#include <unistd.h> // for usleep
#include <cmath>
#include <signal.h>
#include <getopt.h>

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

const char short_options[] = "d:s:i:a:y:f:w:h:l";

const struct option long_options[] = {
    { "device", required_argument, NULL, 'd' },
    { "send", required_argument, NULL, 's' },
    { "interface", required_argument, NULL, 'i' },
    { "ackinter", required_argument, NULL, 'a' },
    { "file", required_argument, NULL, 'y' },
    { "listen", no_argument, NULL, 'l' },
    { 0, 0, 0, 0 }
};

void usage(FILE *fp, int argc, char **argv)
{
    fprintf(fp,
            "Usage: %s [options]\n\n"
            "Options:\n"
            "-d | --device name 	Video device name\n"
            "-s | --ip address  	Remote host IPv4\n"
            "-i | --interface   	Network interface\n"
            "-a | --ACK interface   ACK interface\n"
            "-y | --YUV path  		Path to YUV file\n"
            "-f | --YUV fps   		Fps of YUV file\n"
            "-w | --YUV width   	Width of YUV file\n"
            "-h | --YUV height  	Height of YUV file\n"
            "-l | 				    Listen for incoming\n"
            "",
            argv[0] );
}

int main ( int argc, char* argv[] )
{
	signal( SIGINT, sigint_handler );
	const char* device = "/dev/video0", *remote_ipv4 = "0.0.0.0";
    const char* interface = NULL, *ackinter = NULL;
    int b_webcam = 1, fps = 0, w = 0, h = 0;
    v4l2capture* vidcap = NULL;
    FILE* yuvfile = NULL;  
    
    /* read the input arguments */
    for (;;) {
        int idx;
        int c;
        
        c = getopt_long(argc, argv, short_options, long_options, &idx);
        
        if (-1 == c)
            break;
        
        switch (c) {
            case 0: /* getopt_long() flag */
                break;
                
            case 'd':
                device = optarg;
                break;
                
            case 's':
				remote_ipv4 = optarg;
				break;
				
            case 'i':
				interface = optarg;
				break;
				
            case 'a':
				ackinter = optarg;
				break;	
				
            case 'y':
				yuvfile = fopen( optarg, "rb" );
				b_webcam = 0;
				break;											

            case 'f':
				fps = atoi(optarg);
				break;		
                
            case 'w':
				w = atoi(optarg);
				break;		                

            case 'h':
				h = atoi(optarg);
				break;		
				                
            default:
				usage(stderr, argc, argv);
                exit(EXIT_FAILURE);
        }
    }	
	
	ReberaEncoder encoder;
	encoder.reduce_fr_by( 1 );
	
	SenderState sender( remote_ipv4, interface, ackinter );
	sender.set_ipr( encoder.get_ipr() );

	/* we catch events (arrival of acks, feedback packets, or new frames) using select */
	Select &sel = Select::get_instance();
	sel.add_fd( sender.socket.get_sock() );
	sel.add_fd( sender.acksocket.get_sock() );
	
	if ( b_webcam ) {
		vidcap = new v4l2capture( device );
		encoder.attach_camera( vidcap );
		vidcap->set_picbuff( encoder.get_Ybuff(), encoder.get_Cbbuff(), encoder.get_Crbuff() );
		sel.add_fd( vidcap->get_fd() );
		vidcap->start_capturing();		
	} else {
		encoder.attach_file( yuvfile, w, h );
	}
	
	uint64_t start = 0, next = 0;
	while ( !b_ctrl_c )
	{
		SDL_Event evt;
		while( SDL_PollEvent( &evt ) ) //Returns 1 if there's a pending event, 0 if there are none
		{
			switch( evt.type ) {
				case SDL_QUIT :
					b_ctrl_c = 1;
					goto EXIT;
				default :
					break;
			}
		}
		
		if ( b_webcam )
		{
			sel.select( -1 );
		}
		else
		{
			uint64_t now = GetTimeNow();
			if ( !start ) start = now;
			sel.select( start + next >= now ? start + next - now : 0 );
		}

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
				if ( !b_webcam ) next = (1000000/fps) * encoder.i_enc_frame;
			}
		}
	}
EXIT:
	if ( b_webcam ) vidcap->stop_capturing();

return 0;
}
