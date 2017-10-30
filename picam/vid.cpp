#include "vid.h"
#include "bcm_host.h"
#include "interface/vcos/vcos.h"
#include "RaspiCamControl.h"
//#include "RaspiCLI.h"
#include "vid_util.h"
// Video format information
// 0 implies variable
#define VIDEO_FRAME_RATE_NUM 30
#define VIDEO_FRAME_RATE_DEN 1
#define STILLS_FRAME_RATE_NUM 0
#define STILLS_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

#define SPLITTER_OUTPUT_PORT 0
#define SPLITTER_PREVIEW_PORT 1

MMAL_PORT_T* preview_input_port = NULL;
MMAL_PORT_T *splitter_input_port = NULL;
MMAL_PORT_T *splitter_output_port = NULL;
MMAL_PORT_T *splitter_preview_port = NULL;

// Max bitrate we allow for recording
const int MAX_BITRATE_MJPEG = 25000000; // 25Mbits/s
const int MAX_BITRATE_LEVEL4 = 25000000; // 25Mbits/s
const int MAX_BITRATE_LEVEL42 = 62500000; // 62.5Mbits/s
void Vid::setSensorDefaults(){
	//Gests camera name, and sets res to 2592x1944
}
//Private
void Vid::create_camera_component()
{
	//TODO look at simplfying these options and have them change at run time
	/*
	eg 
	sensor mode
	change_event_request
	camera control
	*/
	MMAL_ES_FORMAT_T* format;
	MMAL_STATUS_T status;
	/* Create the component */
	status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera_component);

	int rc = raspicamcontrol_set_stereo_mode(camera_component->output[0], &this->camera_parameters.stereo_mode);
	rc += raspicamcontrol_set_stereo_mode(camera_component->output[1], &this->camera_parameters.stereo_mode);
	rc += raspicamcontrol_set_stereo_mode(camera_component->output[2], &this->camera_parameters.stereo_mode);
	if (status != MMAL_SUCCESS) {
	   vcos_log_error("Could not set stereo mode : error %d", rc);
	   throw CameraException("Could not set stereo mode");
	}

	if (status != MMAL_SUCCESS) throw CameraException(status, "Failed to create camera component");
	setParameterUint32(camera_component->control, MMAL_PARAMETER_CAMERA_NUM, 0); //TODO
	if (!camera_component->output_num) throw CameraException(MMAL_ENOSYS, "Could not select camera");
	setParameterUint32(camera_component->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, this->sensor_mode);

	preview_port = camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
	video_port = camera_component->output[MMAL_CAMERA_VIDEO_PORT];
	still_port = camera_component->output[MMAL_CAMERA_CAPTURE_PORT];

	//TODO camera control callback maby
	//if (this->settings) {
	//setParameterBool(camera->control, MMAL_PARAMETER_CHANGE_EVENT_REQUEST, true, SWALLOW_ERROR);
	//}
	//TODO: Figureout what camera control is for
	// Enable the camera, and tell it its control callback function
	//status = mmal_port_enable(camera->control, camera_control_callback);
	//if (status != MMAL_SUCCESS) {
	//	vcos_log_error("Unable to enable control port : error %d", status);
	//	goto error;
	//}

	//  set up the camera configuration
	{
		MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
			{ MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
			.max_stills_w = this->width,
			.max_stills_h = this->height,
			.stills_yuv422 = 0,
			.one_shot_stills = 0,
			.max_preview_video_w = width, 
			//.max_preview_video_w = preview_parameters.previewWindow.width,
			.max_preview_video_h = height,
			//.max_preview_video_h = preview_parameters.previewWindow.height,
			.num_preview_video_frames = 3 + vcos_max(0, (this->framerate - 30) / 10), //TODO figureout if just 3 is better
			.stills_capture_circular_buffer_height = 0,
			.fast_preview_resume = 0,
			.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC
		};
		
		if(mode == STILL || mode == STILL_SPLITTER) { 
			//TODO check if this matters, apparently not
			//cam_config.one_shot_stills = 1; //TODO what is this?
			//cam_config.num_preview_video_frames = 3;
			//cam_config.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC;
		}
		mmal_port_parameter_set(camera_component->control, &cam_config.hdr);
	}
	raspicamcontrol_set_all_parameters(camera_component, &this->camera_parameters); 

	// Now set up the port formats

	// Set the encode format on the Preview port
	// HW limitations mean we need the preview to be the same size as the required recorded output

	if (this->camera_parameters.shutter_speed > 6000000) {
		setParameterFPSRange(preview_port, MMAL_PARAMETER_FPS_RANGE, 50, 1000, 166, 1000);
	} else if (this->camera_parameters.shutter_speed > 1000000) {
		setParameterFPSRange(preview_port, MMAL_PARAMETER_FPS_RANGE, 167, 1000, 999, 1000);
	}

	//enable dynamic framerate if necessary
	if (this->camera_parameters.shutter_speed) {
		if (this->framerate > 1000000. / this->camera_parameters.shutter_speed) {
			this->framerate = 0;
		}
	}

	format = preview_port->format;
	format->encoding = MMAL_ENCODING_OPAQUE;
	format->encoding_variant = MMAL_ENCODING_I420;
	format->es->video.width =  VCOS_ALIGN_UP(width, 32);
	//format->es->video.width = VCOS_ALIGN_UP(preview_parameters.previewWindow.width, 32);
	format->es->video.height = VCOS_ALIGN_UP(height, 16); 
	//format->es->video.height = VCOS_ALIGN_UP(preview_parameters.previewWindow.height, 16);
	format->es->video.crop.x = 0;
	format->es->video.crop.y = 0;
	format->es->video.crop.width = width; 
	//format->es->video.crop.width = preview_parameters.previewWindow.width;
	format->es->video.crop.height = height;  
	//format->es->video.crop.height = preview_parameters.previewWindow.height;
	format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM; //0
	format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN; //1


	// Set the encode format on the video  port

	if (this->camera_parameters.shutter_speed > 6000000) {
		setParameterFPSRange(video_port, MMAL_PARAMETER_FPS_RANGE, 50, 1000, 166, 1000);
	} else if (this->camera_parameters.shutter_speed > 1000000) {
		setParameterFPSRange(video_port, MMAL_PARAMETER_FPS_RANGE, 167, 1000, 999, 1000);
	}

	format = video_port->format;
	format->encoding = MMAL_ENCODING_OPAQUE;
	format->encoding_variant = MMAL_ENCODING_I420;
	format->es->video.width = VCOS_ALIGN_UP(this->width, 32);
	format->es->video.height = VCOS_ALIGN_UP(this->height, 16);
	format->es->video.crop.x = 0;
	format->es->video.crop.y = 0;
	format->es->video.crop.width = this->width;
	format->es->video.crop.height = this->height;
	format->es->video.frame_rate.num = this->framerate;
	format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN; //1
	if(mode == STILL || mode == STILL_SPLITTER) { //TODO: Blocks if not set, find out why
		format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM; //0
		format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_DEN; //1
	}
	// Ensure there are enough buffers to avoid dropping frames
	if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) //TODO maby move this after format_commit
		video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

		
		
	// Set the encode format on the still port
	//TODO check if the fps range was needed?
	if (this->camera_parameters.shutter_speed > 6000000) {
		setParameterFPSRange(still_port, MMAL_PARAMETER_FPS_RANGE, 50, 1000, 166, 1000);
	} else if (this->camera_parameters.shutter_speed > 1000000) {
		setParameterFPSRange(still_port, MMAL_PARAMETER_FPS_RANGE, 167, 1000, 999, 1000);
	}
   
	format = still_port->format;
	format->encoding = MMAL_ENCODING_OPAQUE;
	/*if(mode != STILL && mode != STILL_SPLITTER)*/ format->encoding_variant = MMAL_ENCODING_I420; //TODO apparently does nothing
	format->es->video.width = VCOS_ALIGN_UP(this->width, 32);
	format->es->video.height = VCOS_ALIGN_UP(this->height, 16);
	format->es->video.crop.x = 0;
	format->es->video.crop.y = 0;
	format->es->video.crop.width = this->width;
	format->es->video.crop.height = this->height;
	format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM; //0
	format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN; //1

	/* Ensure there are enough buffers to avoid dropping frames */
	if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM; //TODO maby move this after format_commit
}
void Vid::enable_camera_component() {
	MMAL_STATUS_T status = mmal_port_format_commit(preview_port);
	if (status != MMAL_SUCCESS) throw CameraException(status, "camera viewfinder format couldn't be set");
	status = mmal_port_format_commit(video_port);
	if (status != MMAL_SUCCESS) throw CameraException(status, "camera video format couldn't be set");
	status = mmal_port_format_commit(still_port);
	if (status != MMAL_SUCCESS) throw CameraException(status, "camera still format couldn't be set");
	
	// Ensure there are enough buffers to avoid dropping frames
	if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
	if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
	if (preview_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) preview_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
	status = mmal_component_enable(camera_component);	
	if (status != MMAL_SUCCESS) throw CameraException(status, "camera component couldn't be enabled");
}
void Vid::default_status(){
	this->width = 1920; // Default to 1080p
	this->height = 1080;
	this->bCapturing = 0;
	this->framerate = 30; //TODO constant
	this->cameraNum = 0;
	this->settings = 0;
	this->sensor_mode = 0;

	// Setup preview window defaults
	raspipreview_set_defaults(&this->preview_parameters);

	// Set up the camera_parameters to default
	raspicamcontrol_set_defaults(&this->camera_parameters);
}
void Vid::create_splitter_component(int port_number)
{
	MMAL_STATUS_T status;
	if (camera_component == NULL) throw CameraException(MMAL_ENOSYS, "Camera component must be created before splitter");
	/* Create the component */
	status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_SPLITTER, &splitter_component);
	if (status != MMAL_SUCCESS) throw CameraException(status, "Failed to create splitter component");
	if (!splitter_component->input_num) throw CameraException(MMAL_ENOSYS , "Splitter doesn't have any input port");
	if (splitter_component->output_num < 2) throw CameraException(MMAL_ENOSYS , "Splitter doesn't have enough output ports");

	/* Ensure there are enough buffers to avoid dropping frames: */
	mmal_format_copy(splitter_component->input[0]->format, this->camera_component->output[port_number]->format);

	if (splitter_component->input[0]->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
	splitter_component->input[0]->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

	status = mmal_port_format_commit(splitter_component->input[0]);
	if (status != MMAL_SUCCESS) throw CameraException(status, "Unable to set format on splitter input port");

	/* Splitter can do format conversions, configure format for its output port: */
	for (int i = 0; i < splitter_component->output_num; i++) {
		mmal_format_copy(splitter_component->output[i]->format, splitter_component->input[0]->format);
	}
}
void Vid::enable_splitter_component() {
	MMAL_STATUS_T status;
	splitter_input_port = this->splitter_component->input[0];
	splitter_output_port = this->splitter_component->output[SPLITTER_OUTPUT_PORT];
	splitter_preview_port = this->splitter_component->output[SPLITTER_PREVIEW_PORT];

	if(this->splitter_component){
		for (int i = 0; i < splitter_component->output_num; i++){
			status = mmal_port_format_commit(splitter_component->output[i]);
			if (status != MMAL_SUCCESS){
				vcos_log_error("Unable to set format on splitter output port %d", i);
				throw CameraException( status, "Unable to set format on splitter output port");
			}
		}
	}
	
	if(this->splitter_component){
		status = mmal_component_enable(splitter_component);
		if (status != MMAL_SUCCESS) throw CameraException(status, "splitter component couldn't be enabled");
		if(this->splitter_component){
			status = connect_ports(camera_component->output[splitter_port_num], splitter_input_port, &this->splitter_connection);
			if(status != MMAL_SUCCESS) throw CameraException(status, "Error splitter to camera preview port");
		}
	}
}
//Public
Vid::Vid(){
	default_status();
	this->encoder = NULL;
	this->raw = NULL;
	this->preview_parameters.previewWindow.height = 1024;
	this->preview_parameters.previewWindow.width = 768;
}
EncoderPort* Vid::addEncoder(){
	this->encoder = new EncoderPort(this->width, this->height);
	return this->encoder;
}
RawPort* Vid::addRaw(){
	this->raw = new RawPort(this->width, this->height);
	return this->raw;
}
TexPort* Vid::addTex(){
	this->tex = new TexPort(this->width, this->height);
	return this->tex;
}
ResizePort* Vid::addResize(){
	this->resizer = new ResizePort(this->width, this->height);
	return this->resizer;
}
void Vid::dump_status(){
	dump_component_status(this->camera_component);
	if(preview_parameters.preview_component) dump_component_status(preview_parameters.preview_component);
	if(this->splitter_component) dump_component_status(splitter_component);
	if(this->encoder) dump_component_status(encoder->encoder_component);
	if(this->resizer) dump_component_status(resizer->resize_component);
}
bool Vid::open(){
	 //TODO: figure out what bcm_host does, maby enables videocore communication interface?
	//init broadcom host - QUESTION: can this be called more than once?? ;
	bcm_host_init();
	//int version = raspicamcontrol_get_firmware_version();
	//printf("Firmware version %d\n", version);
	
	MMAL_STATUS_T status = MMAL_SUCCESS;


	//Create components
	create_camera_component();
	splitter_port_num = this->mode;
	if(this->mode < 3) create_splitter_component(splitter_port_num);
	
	MMAL_PORT_T * output_port = NULL;
	if(mode == PREVIEW_SPLITTER) output_port = splitter_component->output[SPLITTER_OUTPUT_PORT];
	if(mode == VIDEO_SPLITTER) output_port = splitter_component->output[SPLITTER_OUTPUT_PORT];
	if(mode == STILL_SPLITTER) output_port = splitter_component->output[SPLITTER_OUTPUT_PORT];
	if(mode == PREVIEW) output_port = preview_port;
	if(mode == VIDEO) output_port = video_port;
	if(mode == STILL) output_port = still_port;

	if(this->raw) raw->setPortFormat(output_port);
	if(this->resizer) resizer->setPortFormat(output_port);
	
	enable_camera_component();

	if(this->splitter_component){
		enable_splitter_component();
	}
	preview_parameters.wantPreview = wantPreview;
	if(!this->tex && this->mode != PREVIEW && this->wantPreview != 3) {
		if ((status = raspipreview_create(&this->preview_parameters)) != MMAL_SUCCESS) throw CameraException(status, "Failed to create preview component");
	}
	//ENABLE
	//--------------------------------------------------------------------------------
	if(this->tex) {
		tex->open();
		tex->connect(output_port);
		tex->enable();
	} 
	if(this->raw){
		raw->open();
		raw->connect(output_port);
		raw->enable();
	}
	if(this->encoder){
		encoder->open();
		encoder->connect(output_port);
		encoder->enable();
	}
	if(this->resizer){
		resizer->open(output_port);
		resizer->connect(output_port);
		resizer->enable();
	}
	if(this->preview_parameters.preview_component && this->wantPreview != 4){
		preview_input_port = this->preview_parameters.preview_component->input[0];
		if(this->splitter_component && splitter_port_num == MMAL_CAMERA_PREVIEW_PORT){
			status = connect_ports(splitter_preview_port, preview_input_port, &this->preview_connection);
			logError(status, "Error connecting splitter to preview");
		} else {
			status = connect_ports(preview_port, preview_input_port, &this->preview_connection);
			logError(status, "Error camera to preview");
		}
		if (status != MMAL_SUCCESS) preview_connection = NULL;
	} else {
		status = MMAL_SUCCESS;
	}
	if(status != MMAL_SUCCESS){
		vcos_log_error("%s: Failed to setup preview output", __func__);
	}
	//dump_status();
	//Start Capture
	this->bCapturing = true;
	if(mode == STILL || mode == STILL_SPLITTER) status = mmal_port_parameter_set_boolean(still_port, MMAL_PARAMETER_CAPTURE, this->bCapturing);
	else status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_CAPTURE, this->bCapturing);
	if(status != MMAL_SUCCESS){
		logError(status, "Error starting capture"); //TODO how to handle
	}
	return true;
}
void Vid::close(){

	// Disable all our ports that are not handled by connections
	MMAL_STATUS_T status;
	if(mode == STILL || mode == STILL_SPLITTER){
		if(video_port && video_port->is_enabled){
			 status = mmal_port_disable(video_port);
			logError(status, "Error disabling video port");
		}
	} else{
		if(still_port && still_port->is_enabled) {
			status = mmal_port_disable(still_port);
			logError(status, "Error disabling still port");
		}
	}
	if(this->encoder) this->encoder->checkDisable();

	if(this->tex) {
		delete this->tex;
		this->tex = NULL;
	}
	if(this->raw) {
		delete this->raw;
		this->raw = NULL;
	}
	if(this->resizer) {
		delete this->resizer;
		this->resizer = NULL;
	}
	//destroy connections
	if (this->preview_parameters.wantPreview && this->preview_connection){
		status = mmal_connection_destroy(this->preview_connection);
		logError(status, "Error destroying preview connection");
	}
	this->preview_connection = NULL;
	if(this->encoder) this->encoder->destroyConnection();
	if(this->splitter_connection) {
		status = mmal_connection_destroy(this->splitter_connection); //connection from camera to splitter
		logError(status, "Error destroying splitter connection");
	}
	this->splitter_connection = NULL;
	//TODO: Why are we both disabling and destroying components?
	/* Disable components */
	if(this->encoder) this->encoder->disableComponent();
	if(this->preview_parameters.preview_component) {
		status = mmal_component_disable(this->preview_parameters.preview_component);
		logError(status, "Error disabling preview component");
	}
	if(this->splitter_component) {
		status = mmal_component_disable(this->splitter_component);
		logError(status, "Error disabling splitter component");
	}
	if(this->camera_component)	{
		status = mmal_component_disable(this->camera_component);
		logError(status, "Error disabling camera component");
	}
	//Destory components
	if(this->encoder) this->encoder->destroyComponent();
	if(this->preview_parameters.preview_component) raspipreview_destroy(&this->preview_parameters);
	if(this->splitter_component){
			status = mmal_component_destroy(this->splitter_component);
			this->splitter_component = NULL;
			logError(status, "Error destroying splitter component");
	}
	if(this->camera_component)	{
		status = mmal_component_destroy(this->camera_component);
		this->camera_component = NULL;
		logError(status, "Error destroying camera component");
	}

	if(this->encoder) {
		delete this->encoder;
		this->encoder = NULL;
	}
}
Vid::~Vid(){
	this->close();	
}
bool Vid::enablePreview(PreviewMode mode){
	MMAL_STATUS_T status;
	if(!this->camera_component){
		this->preview_parameters.wantPreview = mode != PreviewMode::disabled;
		this->preview_parameters.wantFullScreenPreview = mode != windowed;
		return true;	
	}
	if(mode == fullscreen && (!this->preview_parameters.wantPreview || !this->preview_parameters.wantFullScreenPreview)){
		//Destroy nullsink
		if (this->preview_connection) mmal_connection_destroy(this->preview_connection);
		raspipreview_destroy(&this->preview_parameters);
		this->preview_parameters.wantPreview = true;
		this->preview_parameters.wantFullScreenPreview = true;
		status = raspipreview_create(&this->preview_parameters);
		status = connect_ports(preview_port, preview_input_port, &this->preview_connection);
	} else if(mode == windowed && (!this->preview_parameters.wantPreview || this->preview_parameters.wantFullScreenPreview)){
		if (this->preview_connection) mmal_connection_destroy(this->preview_connection);
		raspipreview_destroy(&this->preview_parameters);
		this->preview_parameters.wantPreview = true;
		this->preview_parameters.wantFullScreenPreview = false;
		status = raspipreview_create(&this->preview_parameters);
		status = connect_ports(preview_port, preview_input_port, &this->preview_connection);
	} if(mode == disabled && this->preview_parameters.wantPreview){
		//Destroy preview
		if (this->preview_connection) mmal_connection_destroy(this->preview_connection);
		raspipreview_destroy(&this->preview_parameters);
		this->preview_parameters.wantPreview = mode;
		status = raspipreview_create(&this->preview_parameters);
	}
	return status;
}
bool Vid::setImageFX(int effect){
	return raspicamcontrol_set_imageFX(this->camera_component, (MMAL_PARAM_IMAGEFX_T)effect);
}
bool Vid::setResolution(int width, int height){
	/*this->bCapturing = false;
	MMAL_STATUS_T status = MMAL_SUCCESS;
	this->width = width;
	this->height = height;
	if(encoder_output_port && encoder_output_port->is_enabled) mmal_port_disable(encoder_output_port);
	if(this->encoder_connection){
		status =  mmal_connection_destroy(this->encoder_connection);
		logError(status, "Error destroying encoder_connection");
	}
	if(this->encoder_pool) {
		mmal_port_pool_destroy(this->encoder_component->output[0], this->encoder_pool);
	}
	if(this->encoder_component) {
		status= mmal_component_destroy(this->encoder_component);	
		logError(status, "Error destroying encoder_component");
	}
	logError(status, "Error disabling camera_video_port");
	MMAL_ES_FORMAT_T* format = camera_video_port->format;
	format->encoding = MMAL_ENCODING_OPAQUE;
	format->es->video.width = VCOS_ALIGN_UP(this->width, 32);
	format->es->video.height = VCOS_ALIGN_UP(this->height, 16);
	format->es->video.crop.x = 0;
	format->es->video.crop.y = 0;
	format->es->video.crop.width = this->width;
	format->es->video.crop.height = this->height;
	format->es->video.frame_rate.num = this->framerate;
	format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;
	status = mmal_port_format_commit(camera_video_port);
	//create_encoder();*/
}