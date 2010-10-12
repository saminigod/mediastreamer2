/*
mediastreamer2 android video display filter
Copyright (C) 2010 Belledonne Communications SARL (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2/msfilter.h"
#include <android/bitmap.h>

/*defined in msandroid.cpp*/
extern JavaVM *ms_andsnd_jvm;

typedef struct AndroidDisplay{
	JavaVM *vm;
	jobject jbitmap;
	AndroidBitmapInfo bmpinfo;
	ms_SwsContext *sws;
	MSVideoSize vsize;
}AndroidDisplay;

static void android_display_init(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)ms_new0(AndroidDisplay,1);
	ad->vm=ms_andsnd_jvm;
	MS_VIDEO_SIZE_ASSIGN(ad->vsize,CIF);
	f->data=ad;
}

static void android_display_uninit(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	if (ad->sws){
		ms_sws_freeContext (ad->sws);
		ad->sws=NULL;
	}
	ms_free(ad);
}

static void android_display_preprocess(MSFilter *f){
	
}

static void android_display_process(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	MSPicture pic;
	mblk_t *m;

	ms_filter_lock(f);
	if (ad->jbitmap!=0){
		if ((m=ms_queue_peek_last(f->inputs[0]))!=NULL){
			if (ms_yuv_buf_init_from_mblk (&pic,m)==0){
				MSVideoSize wsize={ad->bmpinfo.width,ad->bmpinfo.height};
				MSVideoSize vsize={pic.w, pic.h};
				MSRect vrect;
				MSPicture dest={0};
				void *pixels=NULL;

				if (!ms_video_size_equal(vsize,ad->vsize)){
					ad->vsize=vsize;
					if (ad->sws){
						ms_sws_freeContext (ad->sws);
						ad->sws=NULL;
					}
				}
				
				ms_layout_compute(wsize,vsize,vsize,-1,0,&vrect, NULL);

				if (ad->sws==NULL){
					ad->sws=ms_sws_getContext (vsize.width,vsize.height,PIX_FMT_YUV420P,
					                           vrect.width,vrect.height,PIX_FMT_RGB565,SWS_BILINEAR,NULL,NULL,NULL);
					if (ad->sws==NULL){
						ms_fatal("Could not obtain sws context !");
					}
				}
				if (AndroidBitmap_lockPixels(ad->jenv,ad->jbitmap,&pixels)==0){
					dest.planes[0]=pixels+(vrect.height*vrect.strides)+(vrect.width*2);
					dest.strides[0]=vrect.strides;
					ms_sws_scale (ad->sws,pic.planes,pic.strides,0,pic.h,dest.planes,dest.strides);
					AndroidBitmap_unlockPixels(ad->jenv,ad->jbitmap);
				}else{
					ms_error("AndroidBitmap_lockPixels() failed !");
				}
			}
		}
	}
	ms_filter_unlock(f);
	ms_queue_flush(f->inputs[0]);
	ms_queue_flush(f->inputs[1]);
}

static int android_display_set_bitmap(MSFilter *f, void *arg){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	unsigned long id=*(unsigned long*)arg;
	int err;
	JNIEnv *jenv=NULL;
	
	if (ad->jvm->AttachCurrentThread(&jenv,NULL)!=0){
		ms_error("Could not get JNIEnv");
		return -1;
	}
	ms_filter_lock(f);
	ad->jbitmap=(jobject)id;
	err=AndroidBitmap_getInfo(jenv,ad->jbitmap,&ad->bmpinfo);
	if (err!=0){
		ms_error("AndroidBitmap_getInfo() failed.");
		ad->jbitmap=0;
		ms_filter_unlock(f);
		return -1;
	}
	ms_filter_unlock();
	ms_message("New java bitmap given with w=%i,h=%i,stride=%i,format=%i",
	           ad->bmpinfo.width,ad->bmpinfo.height,ad->bmpinfo.stride,ad->bmpinfo.format);
}

static MSFilterMethod methods[]={
	{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , android_display_set_bitmap },
	{	0, NULL}
};

MSFilterDesc ms_android_display_desc={
	MS_ANDROID_DISPLAY_ID,
	"MSAndroidDisplay",
	MS_FILTER_OTHER,
	NULL,
	2, /*number of inputs*/
	0, /*number of outputs*/
	android_display_init,
	android_display_preprocess,
	android_display_process,
	NULL,
	android_display_uninit
};

