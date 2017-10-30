#ifndef VID_UTIL_H
#define VID_UTIL_H

#include <exception>
#include "mmalincludes.h"
#include <stdio.h>

/*typedef struct { MMAL_FOURCC_T format, const char * name} FormatName;
FormatName formatNames[] = {
	{MMAL_ENCODING_OPAQUE, "OPQV"},
	{MMAL_ENCODING_I420, "I420"},
	{MMAL_ENCODING_I420_SLICE, "S420"},
	#define MMAL_ENCODING_I420_SLICE       MMAL_FOURCC('S','4','2','0')
	{MMAL_ENCODING_OPAQUE, "OPQV"},
	{MMAL_ENCODING_OPAQUE, "OPQV"},
	{MMAL_ENCODING_OPAQUE, "OPQV"},
	{MMAL_ENCODING_OPAQUE, "OPQV"},
	{MMAL_ENCODING_H264, "H264"},
	{MMAL_ENCODING_MJPEG, "MJPG"},
	{MMAL_ENCODING_JPEG, "JPEG"},
	{MMAL_ENCODING_GIF, "GIF"},
	{MMAL_ENCODING_PNG, "PNG"},
	{MMAL_ENCODING_BMP, "BMP"},
*/

static void logError(MMAL_STATUS_T status, const char * about){
	if(status == MMAL_SUCCESS) return;
	const char * statusText = "";
	switch(status){
		case MMAL_SUCCESS: statusText = "Success"; break;
		case MMAL_ENOMEM: statusText = "Out of memory"; break;
		case MMAL_ENOSPC: statusText = "Out of resources (other than memory)"; break;
		case MMAL_EINVAL: statusText = "Argument is invalid"; break;
		case MMAL_ENOSYS: statusText = "Function not implemented"; break;
		case MMAL_ENOENT: statusText = "No such file or directory"; break;
		case MMAL_ENXIO: statusText = "No such device or address"; break;
		case MMAL_EIO: statusText = "I/O error"; break;
		case MMAL_ESPIPE: statusText = "Illegal seek"; break;
		case MMAL_ECORRUPT: statusText = "Data is corrupt \attention FIXME: not POSIX"; break;
		case MMAL_ENOTREADY: statusText = "Component is not ready \attention FIXME: not POSIX"; break;
		case MMAL_ECONFIG: statusText = "Component is not configured \attention FIXME: not POSIX"; break;
		case MMAL_EISCONN: statusText = "Port is already connected"; break;
		case MMAL_ENOTCONN: statusText = "Port is disconnected"; break;
		case MMAL_EAGAIN: statusText = "Resource temporarily unavailable. Try again later"; break;
		case MMAL_EFAULT: statusText = "Bad address"; break;
		case MMAL_STATUS_MAX: statusText = "Unknown error status max"; break;
		default: statusText = "Unknown error"; break;
	}
	vcos_log_error("%s, %d : %s\n", about, status, statusText);
	printf("%s, %d : %s\n", about, status, statusText);
}
static void dump_port_status(MMAL_PORT_T * port){
	MMAL_ES_FORMAT_T *format = port->format;
	MMAL_FOURCC_T en = format->encoding;
	char encodingName[5] = { (char)(en & 0xFF), (char)(en >> 8), (char)(en >> 16), (char)(en >> 24), '\0' };
	en = format->encoding_variant;
	char encodingVariantName[5] = { (char)(en & 0xFF), (char)(en >> 8), (char)(en >> 16), (char)(en >> 24), '\0' };
	en = format->es->video.color_space;
	char color_space[5] = { (char)(en & 0xFF), (char)(en >> 8), (char)(en >> 16), (char)(en >> 24), '\0' };
	printf("\tPort: %s\n", port->name);
	printf("\t\tFormat: encoding=%s, encoding_variant=%s, resolution=%dx%d, crop=%d,%d,%d,%d framerate=%d/%d, pixel_aspect_ratio=%d/%d, color_space=%s, bitrate=%d\n",
		encodingName, encodingVariantName, 
		format->es->video.width,
		format->es->video.height,
		format->es->video.crop.x,
		format->es->video.crop.y,
		format->es->video.crop.width,
		format->es->video.crop.height,
		format->es->video.frame_rate.num,
		format->es->video.frame_rate.den,
		format->es->video.par.num,
		format->es->video.par.den,
		color_space,
		format->bitrate);
	printf("\t\tBuffer: num=%d min=%d rec=%d size=%d min=%d rec=%d\n", 
		port->buffer_num,
		port->buffer_num_min,
		port->buffer_num_recommended,
		port->buffer_size,
		port->buffer_size_min,
		port->buffer_size_recommended);
}
static void dump_component_status(MMAL_COMPONENT_T * component){
	printf("Component: %s\n", component->name);
	for(int i = 0; i < component->input_num; i++){
		MMAL_PORT_T * port = component->input[i];
		dump_port_status(port);
	}
	for(int i = 0; i < component->output_num; i++){
		MMAL_PORT_T * port = component->output[i];
		dump_port_status(port);		
	}
}
class Buffer {
	public:
	uint8_t * data = NULL;
	int length = 0;
	~Buffer() { 
		if(data) delete data;
	}
};
typedef struct PORT_USERDATA
{
	void (*userCallback)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) = NULL;
	MMAL_POOL_T* pool = NULL;
	bool wantCapture = false;
	VCOS_SEMAPHORE_T complete_semaphore;
	Buffer * buffer = NULL;
} PORT_USERDATA;

typedef struct MMAL_PORT_USERDATA_T MMAL_PORT_USERDATA_T;
static void defaultCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer){
	PORT_USERDATA *state = ( PORT_USERDATA * ) port->userdata;
	if(buffer->length) {
		//got data so lock the buffer, call the callback so the application can use it, then unlock
		mmal_buffer_header_mem_lock(buffer);
		if(state->wantCapture && state->buffer != NULL){
			state->buffer->data = new uint8_t[buffer->length];
			memcpy(state->buffer->data, buffer->data, buffer->length);
			state->buffer->length = buffer->length;
			state->wantCapture = false;
			vcos_semaphore_post(&(state->complete_semaphore));
		} else if(state->userCallback) state->userCallback(port, buffer);
		mmal_buffer_header_mem_unlock(buffer);
	}
	// release buffer back to the pool
	mmal_buffer_header_release(buffer);
	// and send one back to the port (if still open)
	if (port->is_enabled && state->pool && state->pool->queue) {
		MMAL_STATUS_T status;
		MMAL_BUFFER_HEADER_T *new_buffer = mmal_queue_get(state->pool->queue);
		if (new_buffer) status = mmal_port_send_buffer(port, new_buffer);
		if (!new_buffer || status != MMAL_SUCCESS) vcos_log_error("Unable to return a buffer to the encoder port");
	 }
}
class CameraException : public std::exception {
	public:
	MMAL_STATUS_T status;
	const char * _message = "";
	explicit CameraException(MMAL_STATUS_T status){

	}
	explicit CameraException(const char * message){
		
	}
	explicit CameraException(MMAL_STATUS_T status, const char * message){  
		_message = message;	   
	}
	virtual ~CameraException() throw (){}
	virtual const char * what () const throw () {
		return _message;
	}
};
enum ErrorHandling { THROW_ERROR, SWALLOW_ERROR }; 
static MMAL_STATUS_T setParameterBool(MMAL_PORT_T * port,  uint32_t type, bool value, ErrorHandling errorHandling = THROW_ERROR){
	MMAL_STATUS_T status;
	switch(type){
		case MMAL_PARAMETER_CHANGE_EVENT_REQUEST:
		{
			MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request = { { MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T) }, MMAL_PARAMETER_CAMERA_SETTINGS, value };
			mmal_port_parameter_set(port, &change_event_request.hdr);
			break;
		}
		default:
		status = mmal_port_parameter_set_boolean(port, type, value);		
	}
	if(status != MMAL_SUCCESS && errorHandling == THROW_ERROR ) throw CameraException(status);
	return status;
}

static MMAL_STATUS_T setParameterUint32(MMAL_PORT_T * port, uint32_t type, uint32_t value, ErrorHandling errorHandling = THROW_ERROR){
	MMAL_STATUS_T status;
	switch(type){
		case MMAL_PARAMETER_INTRAPERIOD:
		case MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT:
		case MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT:
		case MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT:
		{
			MMAL_PARAMETER_UINT32_T param = {{ type, sizeof(param)}, value};
			status = mmal_port_parameter_set(port, &param.hdr);
			break;

		}
		default:
		status = mmal_port_parameter_set_uint32(port, type, value);
	}
	if(status != MMAL_SUCCESS && errorHandling == THROW_ERROR) throw CameraException(status);
	return status;
}
static MMAL_STATUS_T setParameterFPSRange(MMAL_PORT_T * port, int32_t type, int32_t a, int32_t b, int32_t c, int32_t d, ErrorHandling errorHandling = THROW_ERROR){
	MMAL_STATUS_T status;
	MMAL_PARAMETER_FPS_RANGE_T fps_range = { { MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range) },
		{ a, b }, { c, d } };
	mmal_port_parameter_set(port, &fps_range.hdr);
	if(status != MMAL_SUCCESS && errorHandling == THROW_ERROR) throw CameraException(status);
	return status;
}
static MMAL_STATUS_T connect_ports(MMAL_PORT_T* output_port, MMAL_PORT_T* input_port, MMAL_CONNECTION_T** connection)
{
	MMAL_STATUS_T status = mmal_connection_create(connection, output_port, input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
	if (status == MMAL_SUCCESS) {
		status = mmal_connection_enable(*connection);
		if (status != MMAL_SUCCESS)
			mmal_connection_destroy(*connection);
	}

	return status;
}
#endif
