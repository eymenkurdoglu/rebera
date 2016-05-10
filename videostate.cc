#include "videostate.hh"

ReberaVideo::ReberaVideo( void )
: m_bRunning( true )
, m_PictureQCond( SDL_CreateCond() )
, m_PictureQMutex( SDL_CreateMutex() )
, FFmpeg_avcodec( avcodec_find_decoder( AV_CODEC_ID_H264 ) )
, m_pFFmpeg_avcodeccontxt( avcodec_alloc_context3( FFmpeg_avcodec ) )
, FFmpeg_avframe( av_frame_alloc() )
{
	m_pFFmpeg_avcodeccontxt->pix_fmt = AV_PIX_FMT_YUV420P;
	if ( avcodec_open2( m_pFFmpeg_avcodeccontxt, FFmpeg_avcodec, NULL ) < 0 ) {
		fprintf( stderr, "> avcodec_open2 failed\n" );
		exit( EXIT_FAILURE );
	}
	m_pAVPacketQ = new ReberaVideo::AVPacketQueue( (ReberaVideo*)this );
}

ReberaVideo::~ReberaVideo(void)
{
	delete m_pAVPacketQ;
	av_frame_free( &FFmpeg_avframe );
	SDL_DestroyCond( m_PictureQCond );
	SDL_DestroyMutex( m_PictureQMutex );
	avcodec_close( m_pFFmpeg_avcodeccontxt );
	av_free( m_pFFmpeg_avcodeccontxt );
}

ReberaVideo::AVPacketQueue::AVPacketQueue( ReberaVideo* _video_ )
: ffmpeg_avpacketlist_head( NULL )
, ffmpeg_avpacketlist_rear( NULL )
, m_nPackets( 0 )
, m_nSize( 0 )
, m_pCond( SDL_CreateCond() )
, m_pMutex( SDL_CreateMutex() )
, m_pVideoState( _video_ )
{}

ReberaVideo::AVPacketQueue::~AVPacketQueue(void)
{
	SDL_DestroyCond( m_pCond );
	SDL_DestroyMutex( m_pMutex );
}

int ReberaVideo::AVPacketQueue::GetAVPacket( AVPacket* pAVPacket ) {

SDL_LockMutex( m_pMutex );
for (;;)
{
	if ( !m_pVideoState->IsRunning() ) return -1;

	AVPacketList* pHeadAVPacket = ffmpeg_avpacketlist_head;

	if ( pHeadAVPacket )
	{
		if ( !(ffmpeg_avpacketlist_head = pHeadAVPacket->next) ) ffmpeg_avpacketlist_rear = NULL;
		m_nPackets--;
		m_nSize -= pHeadAVPacket->pkt.size;

		if ( pHeadAVPacket->pkt.size > 0 ) {
			av_copy_packet( pAVPacket, &(pHeadAVPacket->pkt) );
			av_packet_unref( &(pHeadAVPacket->pkt) );
			av_free( pHeadAVPacket ); // I hope AVPacket inside is freed as well
			break;
		} else {
			av_free( pHeadAVPacket );
			goto _CONTINUE;
		}
	}

_CONTINUE:
	SDL_CondWaitTimeout( m_pCond, m_pMutex, 4000 );
}
SDL_UnlockMutex( m_pMutex );

return pAVPacket->size;
}

void ReberaVideo::AVPacketQueue::PutAVPacket( AVPacket* pPacket ) {

	AVPacketList* newAVPacketList = (AVPacketList*)av_malloc( sizeof(AVPacketList) );

	newAVPacketList->pkt  = *pPacket;
	newAVPacketList->next = NULL;

	SDL_LockMutex( m_pMutex );
	{
		(ffmpeg_avpacketlist_rear ?
			ffmpeg_avpacketlist_rear->next : ffmpeg_avpacketlist_head) = newAVPacketList;
		ffmpeg_avpacketlist_rear = newAVPacketList;
		m_nPackets++;
		m_nSize += newAVPacketList->pkt.size;
		SDL_CondSignal( m_pCond );
	}
	SDL_UnlockMutex( m_pMutex );
}

void ReberaVideo::InitializeSDLWindow() {

	fprintf( stderr, "> Video size = %dx%d\n", GetVideoWidth(), GetVideoHeight() );

	if ( NULL == (m_pWindow = SDL_CreateWindow( "Rebera Receiver - NYU Video Lab", 800, 200, GetVideoWidth(), GetVideoHeight(), SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE ) ))
		fprintf( stderr, "Window could not be created! SDL Error: %s\n", SDL_GetError() );

	if ( NULL == (m_pRenderer = SDL_CreateRenderer( m_pWindow, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_TARGETTEXTURE)) )
	//if ( NULL == (m_pRenderer = SDL_CreateRenderer( m_pWindow, -1, SDL_RENDERER_TARGETTEXTURE)) )
		fprintf( stderr, "Renderer could not be created! SDL Error: %s\n", SDL_GetError() );

	if ( NULL == (m_pTexture = SDL_CreateTexture( m_pRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, GetVideoWidth(),GetVideoHeight()) ) )
		fprintf( stderr, "Unable to create blank texture! SDL Error: %s\n", SDL_GetError() );
}

void ReberaVideo::refresh_display()
{
	AVFrame* pFrame = FFmpeg_avframe;
	if ( 0 > SDL_UpdateYUVTexture(	GetSDLTexture(), NULL,
									pFrame->data[0], pFrame->linesize[0],
									pFrame->data[1], pFrame->linesize[1],
									pFrame->data[2], pFrame->linesize[2] ) )
		fprintf( stderr, "Could not update YUV texture - %s\n", SDL_GetError());
	else {
		SDL_RenderClear( GetSDLRenderer() );
		SDL_RenderCopy( GetSDLRenderer(), GetSDLTexture(), NULL, NULL );
		SDL_RenderPresent( GetSDLRenderer() );
	}
	av_frame_unref( pFrame );
}

int ReberaVideo::decode_frame( AVPacket* _avpacket )
{
	int b_decoded = 0;
	if ( _avpacket )
	{
		int got_picture;
		if ( avcodec_decode_video2( m_pFFmpeg_avcodeccontxt, FFmpeg_avframe, &got_picture, _avpacket ) < 0 )
		{
			fprintf( stderr, "> Could not decode frame\n" );
		} else {
			b_decoded = 1;
			if ( !b_gotdimensions ) {
				//SDL_Event evt;
				//evt.type = RBR_INITWND;
				//SDL_PushEvent( &evt );
				InitializeSDLWindow();
				b_gotdimensions = 1;
			}
		}
		av_packet_unref( _avpacket );
	}
return b_decoded;
}