#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <thread>

#include "picam/vid.h"
#include "picam/vid_util.h"
#include "gl_scene.h"
using namespace std;
enum TestMode {
	H264,
	MJPEG,
	
	PNG,
	JPEG,
	BMP,
	GIF,

	YUV,
	RGB,

	TEX,
	RESIZE_ISP,
	RESIZE_VPU,

	JPEG_NO_PREVIEW,
	MODE_NR_ITEMS,
};
int frameCount = 0;
void callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer){
	frameCount++;
}
/*
export VC_LOGLEVEL="mmal:trace"
serious errors
2.4	os crash
4.4 os crash
5.4 os crash
medium
8.2		something doesn't return, can't restart properly
11.5 	something doesn't return, can't restart properly
minor errors
1.0 can't close camera
1.3	can't close camera
2.1	155773.898: vcos_abort: Halting	155773.857: assert( *p==0xa55a5aa5 ) failed; ../../../../../vcfw/rtos/common/rtos_common_malloc.c::rtos_pool_aligned_free line 205 rev a3d7660
3.4	153685.522: vcos_abort: Halting	153685.468: assert( ! (IS_ALIAS_NORMAL(Y) || IS_ALIAS_NORMAL(U) || IS_ALIAS_NORMAL(V)) ) failed; ../../../../../codecs/image/jpeghw/codec/jpe_enc.c::jpe_enc_rectangle line 295 rev a3d7660
4.1	133863.024: vcos_abort: Halting	133862.975: assert( *p==0xa55a5aa5 ) failed; ../../../../../vcfw/rtos/common/rtos_common_malloc.c::rtos_pool_aligned_free line 205 rev a3d7660
*/
int delay = 5;
void runTest(TestMode mode, PortMode port_mode){
	printf("test %d.%d\n", mode, port_mode);
	Vid vid;
	vid.mode = port_mode;
	if(mode == H264){
		printf("H264\n");
		EncoderPort * encoder = vid.addEncoder();
		if(port_mode == STILL || port_mode == STILL_SPLITTER) {
			encoder->framerate = 12;
			return; //Error enabling connection
		}
		encoder->encoding = MMAL_ENCODING_H264;
		encoder->callback = callback;
	}
	if(mode == MJPEG){
		printf("MJPEG\n");
		//if(port_mode == PREVIEW_SPLITTER) return;
		//if(port_mode == PREVIEW) return; //freezes badly
		EncoderPort * encoder = vid.addEncoder();
		encoder->encoding = MMAL_ENCODING_MJPEG;
		encoder->callback = callback;
	}
	if(mode == JPEG){
		printf("JPEG\n");
		//if(port_mode == VIDEO) return;
		EncoderPort * encoder = vid.addEncoder();
		encoder->encoding = MMAL_ENCODING_JPEG;
		encoder->callback = callback;
	}
	if(mode == JPEG_NO_PREVIEW){
		printf("JPEG\n");
		//if(port_mode == VIDEO) return;
		EncoderPort * encoder = vid.addEncoder();
		encoder->encoding = MMAL_ENCODING_JPEG;
		encoder->callback = callback;
		vid.wantPreview = 3; //TODO make enum NO_PREVIEW_COMPONENT
	}
	if(mode == PNG){
		printf("PNG\n");
		//if(port_mode == VIDEO_SPLITTER) return;
		//if(port_mode == VIDEO) return;
		EncoderPort * encoder = vid.addEncoder();
		encoder->encoding = MMAL_ENCODING_PNG;
		encoder->callback = callback;
	}
	if(mode == BMP){
		printf("BMP\n");
		//if(port_mode == VIDEO_SPLITTER) return;
		//if(port_mode == VIDEO) return;
		EncoderPort * encoder = vid.addEncoder();
		encoder->encoding = MMAL_ENCODING_BMP;
		encoder->callback = callback;
	}
	if(mode == GIF){
		printf("Gif\n");
		//if(port_mode == VIDEO) return;
		EncoderPort * encoder = vid.addEncoder();
		encoder->encoding = MMAL_ENCODING_GIF;
		encoder->callback = callback;
	}
	if(mode == YUV){
		printf("YUV\n");
		RawPort * raw = vid.addRaw();
		raw->raw_output_fmt = RAW_OUTPUT_FMT_YUV;
		raw->callback = callback;
	}
	if(mode == RGB){
		printf("RGB\n");
		RawPort * raw = vid.addRaw();
		raw->raw_output_fmt = RAW_OUTPUT_FMT_RGB;
		raw->callback = callback;
	}
	if(mode == RESIZE_VPU){
		return; //New userland version killed VPU?
		printf("Resize VPU\n");
		//if(port_mode == PREVIEW) return;
		//if(port_mode == VIDEO) return;
		//if(port_mode == STILL) return;
		ResizePort * resize = vid.addResize();
		resize->resizeMode = ResizePort::VPU;

		resize->callback = callback;
	}
	if(mode == RESIZE_ISP){
		printf("Resize ISP\n");
		ResizePort * resize = vid.addResize();
		resize->resizeMode = ResizePort::ISP;
		resize->callback = callback;
	}
	TexPort * tex;
	if(mode == TEX){
		printf("Tex\n");
		tex = vid.addTex();
		tex->callback = gl_callback;
	}
	vid.open();
	if(mode == TEX) gl_start(tex->port, tex->pool);
	this_thread::sleep_for(chrono::seconds(delay));
	printf("Frame Count %d\n", frameCount);
	if(mode == TEX) gl_stop();
}
int main(int argc, const char * argv[]){
	printf("Hello world!!!\n");
	int x = 0;
	int y = 0;
	if(argc >= 3){
		x = atoi(argv[1]);
		y = atoi(argv[2]);
	}
	runTest((TestMode)x, (PortMode)y);
}

