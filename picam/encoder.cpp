#include "encoder.h"
#include <stdio.h>
// Max bitrate we allow for recording
const int MAX_BITRATE_MJPEG = 25000000; // 25Mbits/s
const int MAX_BITRATE_LEVEL4 = 25000000; // 25Mbits/s
const int MAX_BITRATE_LEVEL42 = 62500000; // 62.5Mbits/s
// Video format information
// 0 implies variable
#define VIDEO_FRAME_RATE_NUM 30
#define VIDEO_FRAME_RATE_DEN 1


EncoderPort::EncoderPort(int width, int height) {
	this->width = width; // Default to 1080p
	this->height = height;
	this->encoding = MMAL_ENCODING_H264;
	this->bitrate = 17000000; // This is a decent default bitrate for 1080p
	this->framerate = VIDEO_FRAME_RATE_NUM;
	this->intraperiod = -1; // Not set
	this->quantisationParameter = 0;
	this->immutableInput = 1;
	this->profile = MMAL_VIDEO_PROFILE_H264_HIGH;
	this->level = MMAL_VIDEO_LEVEL_H264_4;
	this->bInlineHeaders = 0;
	this->intra_refresh_type = -1;
	this->encoding = MMAL_ENCODING_MJPEG;
}
void EncoderPort::setH264Format(){
	MMAL_STATUS_T status;
	if(this->level == MMAL_VIDEO_LEVEL_H264_4)
	{
		if(this->bitrate > MAX_BITRATE_LEVEL4) 	{
			vcos_log_error("Bitrate too high: Reducing to 25MBit/s\n");
			this->bitrate = MAX_BITRATE_LEVEL4;
		}
	}
	else if(this->bitrate > MAX_BITRATE_LEVEL42)
	{
		vcos_log_error("Bitrate too high: Reducing to 62.5MBit/s\n");
		this->bitrate = MAX_BITRATE_LEVEL42;
	}
}
void EncoderPort::setH264Parameters(){
	MMAL_STATUS_T status;
	if (this->encoding == MMAL_ENCODING_H264 &&	this->intraperiod != -1) setParameterUint32(encoder_output_port, MMAL_PARAMETER_INTRAPERIOD, this->intraperiod);
	if (this->encoding == MMAL_ENCODING_H264 && this->quantisationParameter) {
		setParameterUint32(encoder_output_port, MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, this->quantisationParameter);
		setParameterUint32(encoder_output_port, MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, this->quantisationParameter);
		setParameterUint32(encoder_output_port, MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, this->quantisationParameter);
	}

	MMAL_PARAMETER_VIDEO_PROFILE_T  param;
	param.hdr.id = MMAL_PARAMETER_PROFILE;
	param.hdr.size = sizeof(param);
	param.profile[0].profile = this->profile;
	if((VCOS_ALIGN_UP(this->width,16) >> 4) * (VCOS_ALIGN_UP(this->height,16) >> 4) * this->framerate > 245760) {
		if((VCOS_ALIGN_UP(this->width,16) >> 4) * (VCOS_ALIGN_UP(this->height,16) >> 4) * this->framerate <= 522240) {
			vcos_log_error("Too many macroblocks/s: Increasing H264 Level to 4.2\n");
			this->level=MMAL_VIDEO_LEVEL_H264_42;
		} else {
			throw CameraException(MMAL_EINVAL, "Too many macroblocks/s requested");
		}
	}
	param.profile[0].level = this->level;
	status = mmal_port_parameter_set(encoder_output_port, &param.hdr);
	if (status != MMAL_SUCCESS) throw CameraException(status, "Unable to set H264 profile");

	if (this->intra_refresh_type != -1)
	{
		MMAL_PARAMETER_VIDEO_INTRA_REFRESH_T  param;
		param.hdr.id = MMAL_PARAMETER_VIDEO_INTRA_REFRESH;
		param.hdr.size = sizeof(param);
	
		// Get first so we don't overwrite anything unexpectedly
		status = mmal_port_parameter_get(encoder_output_port, &param.hdr);
		if (status != MMAL_SUCCESS)
		{
			vcos_log_warn("Unable to get existing H264 intra-refresh values. Please update your firmware\n");
			// Set some defaults, don't just pass random stack data
			param.air_mbs = param.air_ref = param.cir_mbs = param.pir_mbs = 0;
		}
		//TODO whats going on here?
		param.refresh_mode = (MMAL_VIDEO_INTRA_REFRESH_T)this->intra_refresh_type;
		//if (this->intra_refresh_type == MMAL_VIDEO_INTRA_REFRESH_CYCLIC_MROWS) 
		//   param.cir_mbs = 10;
	
		status = mmal_port_parameter_set(encoder_output_port, &param.hdr);
		throw CameraException(status, "Unable to set H264 intra-refresh values");
	}
}
void EncoderPort::setMJPEGFormat(){
	if(this->bitrate > MAX_BITRATE_MJPEG)
	{
		vcos_log_error("Bitrate too high: Reducing to 25MBit/s\n");
		this->bitrate = MAX_BITRATE_MJPEG;
	}
}
void EncoderPort::setJPEGParameters(){
	MMAL_STATUS_T status;
	int quality = 85;
	int restart_interval = 0;
	status = mmal_port_parameter_set_uint32(encoder_output_port, MMAL_PARAMETER_JPEG_Q_FACTOR, quality);
	status = mmal_port_parameter_set_uint32(encoder_output_port, MMAL_PARAMETER_JPEG_RESTART_INTERVAL, restart_interval);

	//TODO thumbnail
	 // Set up any required thumbnail
	 MMAL_PARAMETER_THUMBNAIL_CONFIG_T param_thumb = {{MMAL_PARAMETER_THUMBNAIL_CONFIGURATION, sizeof(MMAL_PARAMETER_THUMBNAIL_CONFIG_T)}, 0, 0, 0, 0};
  	//if ( state->thumbnailConfig.enable && state->thumbnailConfig.width > 0 && state->thumbnailConfig.height > 0 )
	if(true) {
	// Have a valid thumbnail defined
		param_thumb.enable = 1;
		param_thumb.width = 64;
		param_thumb.height = 48;
		param_thumb.quality = 35;
	}
	status = mmal_port_parameter_set(encoder_component->control, &param_thumb.hdr);
	//TODO exif tags
}
void EncoderPort::open(){
	MMAL_STATUS_T status;
	const char * encoder_type = NULL;
	if(this->encoding == MMAL_ENCODING_MJPEG || this->encoding == MMAL_ENCODING_H264) encoder_type = MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER;
	else encoder_type = MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER;
	status = mmal_component_create(encoder_type, &encoder_component);

	if (status != MMAL_SUCCESS) throw CameraException(status, "Unable to create video encoder component");
	if (!encoder_component->input_num || !encoder_component->output_num) throw CameraException(MMAL_ENOSYS, "Video encoder doesn't have input/output ports");

	encoder_input_port = encoder_component->input[0];
	encoder_output_port = encoder_component->output[0];

	// We want same format on input and output: dest, src
	mmal_format_copy(encoder_output_port->format, encoder_input_port->format);

	encoder_output_port->format->encoding = this->encoding;
	if(this->encoding == MMAL_ENCODING_H264) setH264Format();
	if(this->encoding == MMAL_ENCODING_MJPEG) setMJPEGFormat();

	encoder_output_port->format->bitrate = this->bitrate;

	//buffer_size_recommended == 81920
	if (this->encoding == MMAL_ENCODING_H264) encoder_output_port->buffer_size = encoder_output_port->buffer_size_recommended * 2; //TODO Look at this
	else if(this->encoding == MMAL_ENCODING_MJPEG) encoder_output_port->buffer_size = 256<<10;
	else encoder_output_port->buffer_size = encoder_output_port->buffer_size_recommended;
	
	//printf("BUffer recommended size %d  mjpeg %d\n", encoder_output_port->buffer_size_recommended, 256<<10);
	
	if (encoder_output_port->buffer_size < encoder_output_port->buffer_size_min)
		encoder_output_port->buffer_size = encoder_output_port->buffer_size_min;
	
	encoder_output_port->buffer_num = encoder_output_port->buffer_num_recommended;
	
	if (encoder_output_port->buffer_num < encoder_output_port->buffer_num_min)
		encoder_output_port->buffer_num = encoder_output_port->buffer_num_min;

	// We need to set the frame rate on output to 0, to ensure it gets
	// updated correctly from the input framerate when port connected
	if(encoder_type == MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER){ //TODO check if this matters
		encoder_output_port->format->es->video.frame_rate.num = 0;
		encoder_output_port->format->es->video.frame_rate.den = 1;
	}
	
	// Commit the port changes to the output port
	status = mmal_port_format_commit(encoder_output_port);
	if (status != MMAL_SUCCESS) throw CameraException(status, "Unable to set format on video encoder output port");

	if(this->encoding == MMAL_ENCODING_H264) setH264Parameters();
	if(this->encoding == MMAL_ENCODING_JPEG) setJPEGParameters();
	
	if(encoder_type == MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER) { //TODO check if this matters
		setParameterBool(encoder_input_port, MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT, this->immutableInput, SWALLOW_ERROR);
		
		//set INLINE HEADER flag to generate SPS and PPS for every IDR if requested
		setParameterBool(encoder_output_port, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER, this->bInlineHeaders, SWALLOW_ERROR);
		// Adaptive intra refresh settings
	} else {
		mmal_port_parameter_set_boolean(encoder_output_port, MMAL_PARAMETER_EXIF_DISABLE, 1);
	}
	//  Enable component
	status = mmal_component_enable(encoder_component);
	
	if (status != MMAL_SUCCESS) throw CameraException(status, "Unable to enable video encoder component");
	
	/* Create pool of buffer headers for the output port to consume */
	this->encoder_pool = mmal_port_pool_create(encoder_output_port, encoder_output_port->buffer_num, encoder_output_port->buffer_size);
	
	if (!this->encoder_pool)
	{
		vcos_log_error("Failed to create buffer header pool for encoder output port %s", encoder_output_port->name);
		throw CameraException("Failed to create buffer header pool for encoder output port");
	}
}
void EncoderPort::connect(MMAL_PORT_T * port){
	MMAL_STATUS_T status = connect_ports(port, this->encoder_input_port, &this->encoder_connection);
	if(status != MMAL_SUCCESS) throw CameraException(status, "Could not create encoder connection");
	
}
EncoderPort::~EncoderPort(){
	//this->close();
}
void EncoderPort::close(){
	vcos_semaphore_delete(&callbackData.complete_semaphore); //TODO what happens if we close before we create the semaphore?
	if(this->encoder_output_port && this->encoder_output_port->is_enabled) mmal_port_disable(this->encoder_output_port);
	if(this->encoder_connection) mmal_connection_destroy(this->encoder_connection);
	if(this->encoder_component) mmal_component_disable(this->encoder_component);
	
	if(this->encoder_pool)mmal_port_pool_destroy(this->encoder_component->output[0], this->encoder_pool);
	if(this->encoder_component) mmal_component_destroy(this->encoder_component);
	this->encoder_connection= NULL;
	this->encoder_component=NULL;
	this->encoder_pool=NULL;	
	this->callbackData.pool = NULL;
}
void EncoderPort::enable(){
	callbackData.pool = this->encoder_pool;
	callbackData.userCallback = this->callback;
	VCOS_STATUS_T vcos_status = vcos_semaphore_create(&callbackData.complete_semaphore, "picam-sem", 0);
	vcos_assert(vcos_status == VCOS_SUCCESS);

	encoder_output_port->userdata = (MMAL_PORT_USERDATA_T*)&callbackData;
	mmal_port_enable(encoder_output_port, defaultCallback);

	int num = mmal_queue_length(this->encoder_pool->queue);
	for(int i = 0; i < num; i++){
		MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(this->encoder_pool->queue);
		if (!buffer) vcos_log_error("Unable to get a required buffer %d from pool queue", i);
		if (mmal_port_send_buffer(encoder_output_port, buffer)!= MMAL_SUCCESS)
			vcos_log_error("Unable to send a buffer to encoder output port (%d)", i);
	}
}
void EncoderPort::capture(Buffer * buffer){
	callbackData.wantCapture = true;
	callbackData.buffer = buffer;
	vcos_semaphore_wait(&callbackData.complete_semaphore);
}
void EncoderPort::checkDisable(){
	MMAL_STATUS_T status;
	if (encoder_output_port && encoder_output_port->is_enabled) {
		status = mmal_port_disable(encoder_output_port);
		logError(status, "Error disabling encoder port in checkDisable()");
	}
}
void EncoderPort::destroyConnection(){
	MMAL_STATUS_T status;
	if(this->encoder_connection){ status = mmal_connection_destroy(this->encoder_connection);
		this->encoder_connection = NULL;
		logError(status, "Error destroying encoder connection");
	}
}
void EncoderPort::disableComponent(){
	MMAL_STATUS_T status;
	if(this->encoder_connection) {
		status = mmal_component_disable(this->encoder_component);
		logError(status, "Error disabling encoder component");
	}
}
void EncoderPort::destroyComponent(){
	MMAL_STATUS_T status;
	if (this->encoder_pool) {
	   mmal_port_pool_destroy(this->encoder_component->output[0], this->encoder_pool);
	   this->encoder_pool = NULL;
	}
	if (this->encoder_component) {
	   status = mmal_component_destroy(this->encoder_component);
	   this->encoder_component = NULL;
	   logError(status, "Error destroying encoder component");
	}
}