#include "resizer.h"
#include "stdio.h"
ResizePort::ResizePort(int width, int height) {
	this->width = width;
	this->height = height;
}
void ResizePort::setPortFormat(MMAL_PORT_T * output_port){
	MMAL_ES_FORMAT_T * format = output_port->format;
	format->encoding = MMAL_ENCODING_I420;
	format->encoding_variant = MMAL_ENCODING_I420;
}
void ResizePort::open(MMAL_PORT_T * output_port){
	MMAL_STATUS_T status;
	if(resizeMode == ISP) status = mmal_component_create("vc.ril.isp", &resize_component);
	else mmal_component_create("vc.ril.resize", &resize_component);
	if (status != MMAL_SUCCESS) throw CameraException(status, "Unable to create resize component");
	if (resize_component->input_num == 0 || resize_component->output_num == 0){
		printf("Input num %d output num %d\n", resize_component->input_num, resize_component->output_num);
		throw CameraException(MMAL_ENOSYS, "Resize doesn't have input/output ports");
	}
	resize_input_port = resize_component->input[0];
	resize_output_port = resize_component->output[0];

	// We want same format on input and output dest, src
	mmal_format_copy(resize_input_port->format, output_port->format);


	resize_input_port->buffer_num = 3;
	status = mmal_port_format_commit(resize_input_port);
	if(status != MMAL_SUCCESS) throw CameraException(status, "Unable to set format on resize input port");

	mmal_format_copy(resize_output_port->format,resize_input_port->format);

	if (resize_output_port->buffer_size < resize_output_port->buffer_size_min)
		resize_output_port->buffer_size = resize_output_port->buffer_size_min;
	
	resize_output_port->buffer_num = resize_output_port->buffer_num_recommended;
	
	if (resize_output_port->buffer_num < resize_output_port->buffer_num_min)
		resize_output_port->buffer_num = resize_output_port->buffer_num_min;
	
	if(true) //TODO
	{
		//resize_output_port->format->encoding = MMAL_ENCODING_RGBA;
		//resize_output_port->format->encoding_variant = MMAL_ENCODING_RGBA;
	}
	
	resize_output_port->format->es->video.width = width;
	resize_output_port->format->es->video.height = height;
	resize_output_port->format->es->video.crop.x = 0;
	resize_output_port->format->es->video.crop.y = 0;
	resize_output_port->format->es->video.crop.width = width;
	resize_output_port->format->es->video.crop.height = height;
	status = mmal_port_format_commit(resize_output_port);
	if (status != MMAL_SUCCESS) throw CameraException(status, "Unable to set format on resize output port");
	//  Enable component
	
	status = mmal_component_enable(resize_component);
	if (status != MMAL_SUCCESS) throw CameraException(status, "Unable to enable resize component");
	
	if(wantCallback){
		this->resize_pool = mmal_port_pool_create(resize_output_port, resize_output_port->buffer_num, resize_output_port->buffer_size);
		if (!this->resize_pool)
		{
			vcos_log_error("Failed to create buffer header pool for encoder output port %s", resize_output_port->name);
			throw CameraException("Failed to create buffer header pool for encoder output port");
		}
	}
}
void ResizePort::connect(MMAL_PORT_T * port){
	MMAL_STATUS_T status = connect_ports(port, this->resize_input_port, &this->resize_connection);
	if(status != MMAL_SUCCESS) throw CameraException(status, "Could not create resize connection");
}
ResizePort::~ResizePort(){
	this->close();
}
void ResizePort::close(){
	if(this->resize_output_port && this->resize_output_port->is_enabled) mmal_port_disable(this->resize_output_port);
	if(this->resize_connection) mmal_connection_destroy(this->resize_connection);
	if(this->resize_component) mmal_component_disable(this->resize_component);
	if(this->resize_component) mmal_component_destroy(this->resize_component);
	this->resize_connection= NULL;
	this->resize_component=NULL;
}
void ResizePort::checkDisable(){
	// Disable all our ports that are not handled by connections
}
void ResizePort::enable(){
	callbackData.pool = this->resize_pool;
	callbackData.userCallback = this->callback;

	resize_output_port->userdata = (MMAL_PORT_USERDATA_T*)&callbackData;
	mmal_port_enable(resize_output_port, defaultCallback);

	int num = mmal_queue_length(this->resize_pool->queue);
	for(int i = 0; i < num; i++){
		MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(this->resize_pool->queue);
		if (!buffer) vcos_log_error("Unable to get a required buffer %d from pool queue", i);
		if (mmal_port_send_buffer(resize_output_port, buffer)!= MMAL_SUCCESS)
			vcos_log_error("Unable to send a buffer to encoder output port (%d)", i);
	}
	
}