#include "raw.h"
#include "stdio.h"
RawPort::RawPort(int width, int height) {
	this->width = width;
	this->height = height;
	this->raw_output_fmt = RAW_OUTPUT_FMT_RGB;
}
void RawPort::open(){

}
void RawPort::setPortFormat(MMAL_PORT_T * port){
	MMAL_STATUS_T status;
	MMAL_ES_FORMAT_T * format = port->format;
	switch (this->raw_output_fmt)
	{
		case RAW_OUTPUT_FMT_YUV:
		case RAW_OUTPUT_FMT_GRAY: // Grayscale image contains only luma (Y) component
			format->encoding = MMAL_ENCODING_I420;
			format->encoding_variant = MMAL_ENCODING_I420;
			break;
		case RAW_OUTPUT_FMT_RGB:
			format->encoding = MMAL_ENCODING_RGB24;
			format->encoding_variant = 0;  // Irrelevant when not in opaque mode
			break;
		default:
			throw CameraException(MMAL_EINVAL, "unknown raw output format");
	}
}
void RawPort::connect(MMAL_PORT_T * port){
	this->port = port;
}
RawPort::~RawPort(){
	this->close();
}
void RawPort::close(){
	if (port && port->is_enabled) mmal_port_disable(port);
	if(this->pool) mmal_port_pool_destroy(this->port, this->pool);
	this->port=NULL;
	this->pool=NULL;	
	this->callbackData.pool = NULL;
}
void RawPort::checkDisable(){
	if (port && port->is_enabled) mmal_port_disable(port);
	// Disable all our ports that are not handled by connections
}
void RawPort::enable(){
	//if(status != MMAL_SUCCESS) throw CameraException(status, "Unable to set format on raw output port");  
	port->buffer_size = port->buffer_size_recommended;
	port->buffer_num = port->buffer_num_recommended;
	pool = mmal_port_pool_create ( port, port->buffer_num, port->buffer_size );
	if ( !pool ) throw CameraException( "Failed to create buffer header pool for video output port" );

	callbackData.pool = this->pool;
	callbackData.userCallback = this->callback;
	port->userdata = (MMAL_PORT_USERDATA_T*)&callbackData;
	MMAL_STATUS_T status = mmal_port_enable(port, defaultCallback);
	if(status != MMAL_SUCCESS) throw CameraException(status, "Error Setting up raw port callback");
	int num = mmal_queue_length ( this->pool->queue );
	int q;
	for ( q=0; q<num; q++ ) {
		MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get ( this->pool->queue );

		if ( !buffer ) printf("Unable to get a required buffer %d from pool queue\n", q);

		if ( mmal_port_send_buffer ( port, buffer ) != MMAL_SUCCESS )
			printf("Unable to send a buffer to raw output port %d\n", q);
	}
}