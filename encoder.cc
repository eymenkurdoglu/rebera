#include "encoder.hh"
#define RBR_READ_UNTIL 288
x264encoder::x264encoder( int _width_, int _height_ )
: i_frame( 0 )
, readfrom( NULL )
, writeto( fopen( "output.264", "wb" ) )
, i_readuntil( 0 )
{
	init( _width_, _height_ );
}

x264encoder::x264encoder( int _width_, int _height_, FILE* _yuvfile_ )
: i_frame( 0 )
, readfrom( _yuvfile_ )
, writeto( fopen( "output.264", "wb" ) )
, i_readuntil( 0 )
{
	init( _width_, _height_ );
}

x264encoder::~x264encoder() {
	x264_encoder_close( encoder );
	x264_picture_clean( &pic_in );
}

void x264encoder::init( int _width_, int _height_ )
{
	x264_param_default_preset( &param, "veryfast", "zerolatency" );
	/* Configure non-default params */
    param.i_csp = X264_CSP_I420;
    param.b_vfr_input = 0;
    param.i_threads = 1;
	param.i_width = _width_;
	param.i_height = _height_;
	/* Intra refresh */
	param.i_keyint_max = RBR_INTRAPERIOD;
	param.i_frame_reference = RBR_MAX_NUM_REF;
	param.i_scenecut_threshold = 0;
	/* Rate control */
	param.rc.i_rc_method = X264_RC_ABR;
	param.rc.i_bitrate = RBR_INIT_BITRATE;
	param.rc.i_vbv_max_bitrate = RBR_INIT_BITRATE;
	param.rc.i_vbv_buffer_size = param.rc.i_bitrate/10;
	/* For streaming */
	param.b_repeat_headers = 1;
	param.b_annexb = 1;
	//param.i_log_level = X264_LOG_DEBUG;
    /* Apply profile restrictions. */
    x264_param_apply_profile( &param, "high" );
	x264_picture_alloc( &pic_in, param.i_csp, param.i_width, param.i_height );
	encoder = x264_encoder_open( &param );
	//printf("> Encoder initialized\n");
}

void x264encoder::set_input( FILE* _readfrom_ ) {
	readfrom = _readfrom_;
}

int x264encoder::read_frame() {
	unsigned luma_size = param.i_width*param.i_height;
    unsigned chroma_size = luma_size / 4;

    fread( pic_in.img.plane[0], 1, luma_size, readfrom );
	fread( pic_in.img.plane[1], 1, chroma_size, readfrom );
	fread( pic_in.img.plane[2], 1, chroma_size, readfrom );
	pic_in.i_pts = i_frame++;
	i_readuntil++;
	if ( feof( readfrom ) || i_readuntil == RBR_READ_UNTIL ) {
		i_readuntil = 0;
		rewind( readfrom );
		return read_frame();
	}
return 0;
}

int x264encoder::encode_frame()
{
	i_frame_size = x264_encoder_encode( encoder, &nal, &i_nal, &pic_in, &pic_out );

	if( i_frame_size < 0 ) {
		fprintf( stderr, "> x264 error: encoding fail\n" );
		return -1;
	}
	//if( i_frame_size )
		//fwrite( nal->p_payload, i_frame_size, 1, writeto );

return i_frame_size;
}

int x264encoder::change_bitrate( int _newbitrate_ )
{
	x264_encoder_parameters( encoder, &param );

	param.rc.i_bitrate = _newbitrate_;
	param.rc.i_vbv_max_bitrate = _newbitrate_;
	param.rc.i_vbv_buffer_size = param.rc.i_bitrate/10;

return x264_encoder_reconfig( encoder, &param );
}
