#include "encoder.hh"
#define RBR_READ_UNTIL 288

x264encoder::x264encoder( int _width_, int _height_ )
{
	x264_param_default_preset( &param, "veryfast", "zerolatency" );
	
	/* Configure non-default params */
    param.i_csp 		= X264_CSP_I420;
    param.b_vfr_input 	= 0;
    param.i_threads 	= 1;
	param.i_width 		= _width_;
	param.i_height 		= _height_;
	u_luma_size 		= param.i_width * param.i_height; // set for each frame in case SR changes
	u_chroma_size 		= u_luma_size / 4; // set for each frame in case SR changes
	
	/* Intra refresh */
	param.i_keyint_max 			= RBR_INTRAPERIOD;
	param.i_frame_reference 	= RBR_MAX_NUM_REF;
	param.i_scenecut_threshold 	= 0;
	
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
}

x264encoder::~x264encoder() {
	x264_encoder_close( encoder );
	x264_picture_clean( &pic_in );
}

// TODO: what if read_frame or fread fails?
int x264encoder::get_frame()
{
	int will_encode = 0;
	
		/* read from camera */
	if ( vidcap ) {
		vidcap->read_frame();
	} else {
		/* read from a YUV file */
		fread( pic_in.img.plane[0], 1, u_luma_size,   readfrom );
		fread( pic_in.img.plane[1], 1, u_chroma_size, readfrom );
		fread( pic_in.img.plane[2], 1, u_chroma_size, readfrom );
		
		i_readuntil++;

		if ( feof( readfrom ) || i_readuntil == RBR_READ_UNTIL )
		{
			i_readuntil = 0;
			rewind( readfrom );
			get_frame();
		}
	}
	
	/* raw picture eligible for compression */
	if ( i_raw_frame % i_trim == 0 ) 
	{
		will_encode = 1;
		/* display the raw picture to user */
		if ( vidcap )
			vidcap->display_raw();
		
		/* check if a new intra-period should start */
		if ( i_enc_frame - i_last_idr > i_ipr - 1 )
			i_last_idr = i_enc_frame;
		
		i_frame_index = i_enc_frame - i_last_idr;
		
		//i_enc_frame++;
		
		/* //TO BE ABLE TO CHANGE FR 
		if ( !ul_raw_ts ) { 
			ul_raw_ts = GetTimeNow();
		} else {
			ul_now = GetTimeNow();
			d_interval = 0.9 * d_interval + 0.1 * 1e6 / ( ul_now - ul_raw_ts );
			ul_raw_ts = ul_now;
			printf("\x1b[1m\r> [RBR] FR=%2.1f\x1b[0m", d_interval);
		} */
	}
	i_raw_frame++;

return will_encode;
}

int x264encoder::compress_frame()
{
return x264_encoder_encode( encoder, &nal, &i_nal, &pic_in, &pic_out );
}

int x264encoder::change_bitrate( int _newbitrate_ )
{
	_newbitrate_ = _newbitrate_ <= RBR_MIN_ENCODING_RATE ? RBR_MIN_ENCODING_RATE : ( _newbitrate_ >= RBR_MAX_ENCODING_RATE ? RBR_MAX_ENCODING_RATE : _newbitrate_ );
	
	x264_encoder_parameters( encoder, &param );

	param.rc.i_bitrate = _newbitrate_;
	param.rc.i_vbv_max_bitrate = _newbitrate_;
	param.rc.i_vbv_buffer_size = param.rc.i_bitrate/10;

return x264_encoder_reconfig( encoder, &param );
}
