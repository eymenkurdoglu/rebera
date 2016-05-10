#include <iostream>
#include <list>
#include <cstdlib>
#include <set>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_timer.h>
#include <assert.h>

#include "socket.hh"
#include "payload.hh"
#include "videostate.hh"
#include "receiverstate.hh"
#include "util.hh"
#include "select.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/time.h>
}

void push_quit_event( void )
{
	SDL_Event evt;
	evt.type = SDL_QUIT;
	SDL_PushEvent( &evt );
}

int main( int argc, char* argv[] )
{
	av_register_all(); // link against libavformat
	SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER );

	char* ack_if = NULL; if ( argc > 1 ) ack_if = argv[1];
	ReberaVideo 	video;
	ReberaReceiver 	receiver( &video, ack_if );

FILE* pFile = fopen( "/home/eymen/Desktop/receiver_packets.txt", "w" );

	Select &sel = Select::get_instance();
	sel.add_fd( receiver.sink_sock.get_sock() );
	uint64_t start_time = GetTimeNow(), next_feedback_time = 0;

	while ( video.IsRunning() )
	{
		SDL_Event evt;
		while( SDL_PollEvent( &evt ) ) // Returns 1 if there is a pending event or 0 if there are none.
		{
			switch( evt.type ) {
			case SDL_QUIT :
				video.SetQuitFlag();
				SDL_Quit();
				goto _EXIT;
			case RBR_INITWND :
				video.InitializeSDLWindow();
				break;
			case RBR_UPDATEYUV :
				video.refresh_display();
				break;
			default:
				break;
			}
		}

		uint64_t now = GetTimeNow();
		sel.select( start_time+next_feedback_time >= now ? start_time+next_feedback_time-now : 0 );

		if ( sel.read( receiver.sink_sock.get_sock() ) )
		{
			Socket::Packet incoming( receiver.sink_sock.recv() );

			if ( !incoming.payload.size() ) { push_quit_event(); SDL_Delay(500); continue; }

			Packet::Header* h = (Packet::Header*)incoming.payload.data();
			uint64_t arriving_time = GetTimeNow();

			if ( !receiver.is_connected() ) {
				receiver.set_connected();
				receiver.set_remote_addr( incoming.addr );
			}

			Packet* contents = new Packet( incoming.payload.data(), incoming.payload.size() );

			if ( ack_if ) /* send the ack? */
				receiver.ack_sock.send( Socket::Packet( Socket::Address("123.123.123.123",42003), contents->header.GetHeader() ) );
				//receiver.sink_sock.send( Socket::Packet( Socket::Address("216.165.113.111",42003), contents->header.GetHeader() ) );

			/* log receiver_packets.txt? */
			fprintf( pFile, "%lu %u %lu %d\n", arriving_time-start_time, h->sequencenum, incoming.payload.size()+RBR_NETWORK_STACK_HEADER, h->framenumber );

			/* update connection stats */
			receiver.bytes_received += incoming.payload.size()+RBR_NETWORK_STACK_HEADER;
			receiver.bytes_lost = h->sentsofar-receiver.bytes_received;

			/* place the video data in the decoding buffer */
			receiver.place_in_buffer( contents, arriving_time );

			/* if a frame is complete, decode and display it */
			if ( video.decode_frame( receiver.check_and_queue() ) )
				video.refresh_display();
		}
		else /* time to send the next feedback packet */
		{
			receiver.slides_windows();
			if ( receiver.is_connected() )
			{
				Packet::FbHeader h = receiver.get_fb_header();
				Socket::Packet feedback( receiver.get_remote_addr(), std::string( (char*)&h, sizeof(Packet::FbHeader) ) );
				receiver.sink_sock.send( feedback );
			}
			next_feedback_time = (1000000/(RBR_FB_PER_IPER/(16/15)))*receiver.fb_seq_no;
		}
	}

_EXIT:
fclose( pFile );
return 0;
}