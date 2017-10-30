#ifndef VID_H
#define VID_H

#include "mmalincludes.h"
extern "C" {
#include "RaspiPreview.h"
#include "RaspiCamControl.h"
}
#include "encoder.h"
#include "raw.h"
#include "tex.h"
#include "resizer.h"
#include <stdio.h>

/* Paramters
stereo_mode
sensor_mode
fps_range
shutter_speed
MMAL_PARAMETER_CAMERA_CONFIG
	.max_stills_w = state->width,
	.max_stills_h = state->height,
	.stills_yuv422 = 0,
	.one_shot_stills = 0,					//Videomode = 0, Stillmode = 1
	.max_preview_video_w = state->width,	//videomode = state->width stillmode = state->preview_parameters.previewWindow.width
	.max_preview_video_h = state->height,	//videomode = state->height stillmode = state->preview_parameters.previewWindow.height
	.num_preview_video_frames = 3 + vcos_max(0, (state->framerate-30)/10),	//Videomode = 3 + some, Stillmode = 3
	.stills_capture_circular_buffer_height = 0,
	.fast_preview_resume = 0,
	.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC	//Videomode = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC, Stillmode = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
port format
	format->es->video.width = VCOS_ALIGN_UP(state->width, 32); 			//preview = preview.window.width or state->width, video & still = state->width
	format->es->video.height = VCOS_ALIGN_UP(state->height, 16);		//preview = preview.window.height or state->height, video & still = state->height
	format->es->video.crop.x = 0;										//all = 0
	format->es->video.crop.y = 0;										//all = 0
	format->es->video.crop.width = state->width;						//preview = preview.window.width or state->width, video & still = state->width
	format->es->video.crop.height = state->height;						//preview = preview.window.height or state->height, video & still = state->height
	format->es->video.frame_rate.num = FULL_RES_PREVIEW_FRAME_RATE_NUM;	//preview = FULL_RES_PREVIEW_FRAME_RATE_NUM or PREVIEW_FRAME_RATE_NUM, still = STILLS_FRAME_RATE_NUM(0), video = state->framerate(30)
	format->es->video.frame_rate.den = FULL_RES_PREVIEW_FRAME_RATE_DEN;	//preview = FULL_RES_PREVIEW_FRAME_RATE_DEN or PREVIEW_FRAME_RATE_DEN, still = STILLS_FRAME_RATE_DEN(1), video = VIDEO_FRAME_RATE_DEN(1)
*/
enum PortMode {
	PREVIEW_SPLITTER,
	VIDEO_SPLITTER,
	STILL_SPLITTER,
	PREVIEW,
	VIDEO,
	STILL
};
class Vid {
private:
	uint32_t framerate;
	RASPIPREVIEW_PARAMETERS preview_parameters; /// Preview setup parameters
	RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters
	int splitter_port_num;
	MMAL_COMPONENT_T* camera_component = NULL; /// Pointer to the camera component
	MMAL_COMPONENT_T* splitter_component = NULL; /// Pointer to the video splitter component
	MMAL_CONNECTION_T* preview_connection = NULL; /// Pointer to the connection from camera or splitter to preview
	MMAL_CONNECTION_T* splitter_connection = NULL; /// Pointer to the connection from camera to splitter
	MMAL_PORT_T *preview_port = NULL;
	MMAL_PORT_T *video_port = NULL;
	MMAL_PORT_T *still_port = NULL;
	bool bCapturing = false; /// State of capture/pause
	int cameraNum; /// Camera number
	int settings; /// Request settings from the camera
	uint32_t sensor_mode; /// Sensor mode. 0=auto. Check docs/forum for modes selected by other values.
	EncoderPort * encoder = NULL;
	RawPort * raw = NULL;
	TexPort * tex = NULL;
	ResizePort * resizer = NULL;
	void default_status();
	void setSensorDefaults();
	void create_camera_component();
	void enable_camera_component();
	void create_splitter_component(int port_number);
	void enable_splitter_component();
	void dump_status();
public:
	uint32_t width; /// Requested width of image
	uint32_t height; /// requested height of image
	enum PreviewMode { disabled, fullscreen, windowed };
	int mode = 0;
	int wantPreview = false;
	Vid();
	~Vid();
	bool open();
	void close();
	EncoderPort* addEncoder();
	RawPort* addRaw();
	TexPort* addTex();
	ResizePort* addResize();
	bool enablePreview(PreviewMode mode);
	bool setImageFX(int effect);
	bool setResolution(int width, int height);
};
#endif