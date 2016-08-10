#include "encoder.hh"
#define RBR_READ_UNTIL 288
#define RBR_BUFFER_SIZE_COEFF 5

ReberaEncoder::ReberaEncoder( void )
{
	x264_param_default_preset( &param, "veryfast", "zerolatency" );
	
	/* Configure non-default params */
    param.i_csp 		= X264_CSP_I420;
    param.b_vfr_input 	= 0;
    param.i_threads 	= 1;
	param.i_fps_den 	= 1;
	param.i_fps_num 	= 30;
	
	/* Intra refresh */
	param.i_keyint_max 			= 32;
	param.i_frame_reference 	= RBR_MAX_NUM_REF;
	param.i_scenecut_threshold 	= 0;
	
	/* Rate control */
	param.rc.i_rc_method = X264_RC_ABR;
	param.rc.i_bitrate = 120;
	param.rc.i_vbv_max_bitrate = 120;
	param.rc.i_vbv_buffer_size = param.rc.i_bitrate/RBR_BUFFER_SIZE_COEFF;
	
	/* For streaming */
	param.b_repeat_headers = 1;
	param.b_annexb = 1;
	//param.i_log_level = X264_LOG_DEBUG;
	
    /* Apply profile restrictions. */
    //x264_param_apply_profile( &param, "high" );
	//encoder = x264_encoder_open( &param );	
}

void ReberaEncoder::finalize_init( int w, int h ) {
		param.i_width 		= w;
		param.i_height 		= h;
		u_luma_size 		= param.i_width * param.i_height; // set for each frame in case SR changes
		u_chroma_size 		= u_luma_size / 4; // set for each frame in case SR changes
		x264_picture_alloc( &pic_in, param.i_csp, param.i_width, param.i_height ); 
		x264_param_apply_profile( &param, "high" );
		encoder = x264_encoder_open( &param );
}	

ReberaEncoder::~ReberaEncoder() {
	x264_encoder_close( encoder );
	x264_picture_clean( &pic_in );
}

// TODO: what if read_frame or fread fails?
int ReberaEncoder::get_frame()
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
		if ( i_enc_frame - i_last_idr > i_ipr - 1 ) {
			
			/* this is a new IDR */
			i_last_idr = i_enc_frame;
			
			/* has the FR changed? if so, set a new intra-period */
			if ( d_fr >= 22.5 && d_fr <= 45 )
				i_ipr = (int)(d_ipr * 30);
			else if ( d_fr >= 12.25 && d_fr <= 22.5 )
				i_ipr = (int)(d_ipr * 15);
				
			printf("[RBR] IPR=%d ",i_ipr);
		}
		
		i_frame_index = i_enc_frame - i_last_idr;
		
		//TODO: when reading from file don't run this
		if ( !ul_raw_ts ) { 
			ul_raw_ts = GetTimeNow();
		} else {
			uint64_t ul_now = GetTimeNow();
			d_interval = 0.9 * d_interval + 0.1 * ( ul_now - ul_raw_ts );
			d_fr = 1e6/d_interval;
			ul_raw_ts = ul_now;
		}
	}
	i_raw_frame++;

return will_encode;
}

int ReberaEncoder::compress_frame()
{
return x264_encoder_encode( encoder, &nal, &i_nal, &pic_in, &pic_out );
}

int ReberaEncoder::change_bitrate( double _newbitrate_ )
{
	_newbitrate_ = _newbitrate_ <= RBR_MIN_ENCODING_RATE ? RBR_MIN_ENCODING_RATE : ( _newbitrate_ >= RBR_MAX_ENCODING_RATE ? RBR_MAX_ENCODING_RATE : _newbitrate_ );
	//printf("[RBR] target=%.2f\n", _newbitrate_);
	x264_encoder_parameters( encoder, &param );
	
	_newbitrate_ = (int)_newbitrate_;
	
	param.rc.i_bitrate = _newbitrate_;
	param.rc.i_vbv_max_bitrate = _newbitrate_;
	param.rc.i_vbv_buffer_size = param.rc.i_bitrate/RBR_BUFFER_SIZE_COEFF;

return x264_encoder_reconfig( encoder, &param );
}
