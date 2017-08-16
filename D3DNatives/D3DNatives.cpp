// D3DNatives.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfcaptureengine.h>
#include <queue>
#include <evr.h>
#include <codecapi.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

class VideoEncoder {
public:
	IMFTransform* encoder = 0;
	DWORD thebird = 0;
	DWORD isthe = 0;
	DWORD proc_in = 0;
	DWORD proc_out = 0;
	IMFMediaEvent* evt = 0;
	IMFMediaEventGenerator* generator = 0;
	ID3D11Device* dev = 0;
	ID3D11DeviceContext* ctx = 0;
	ID3D11VideoDevice* viddev = 0;
	ID3D11VideoProcessor* processor = 0;
	ID3D11VideoContext* vidcontext = 0;
	ID3D11VideoProcessorEnumerator* enumerator = 0;
	D3D11_VIDEO_PROCESSOR_STREAM streaminfo;
	void(*packetCallback)(unsigned char*, int);
	VideoEncoder(ID3D11Device* dev, ID3D11DeviceContext* ctx, void(*packetCallback)(unsigned char*, int)) :dev(dev), ctx(ctx), packetCallback(packetCallback) {
		MFStartup(MF_VERSION);
		memset(&streaminfo, 0, sizeof(streaminfo));

		dev->QueryInterface(&viddev);
		D3D11_VIDEO_PROCESSOR_CONTENT_DESC procdesc;
		procdesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
		procdesc.InputFrameRate.Numerator = 0;
		procdesc.InputFrameRate.Denominator = 0;
		procdesc.InputWidth = 1920;
		procdesc.InputHeight = 1080;
		procdesc.OutputFrameRate.Numerator = 0;
		procdesc.OutputFrameRate.Denominator = 0;
		procdesc.OutputWidth = 1920;
		procdesc.OutputHeight = 1080;
		procdesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
		viddev->CreateVideoProcessorEnumerator(&procdesc, &enumerator);
		UINT supportflags;
		enumerator->CheckVideoProcessorFormat(DXGI_FORMAT_B8G8R8A8_UNORM,&supportflags);
		//UINT vflags = 0;
		//HRESULT vres = enumerator->CheckVideoProcessorFormat(DXGI_FORMAT_NV12,&vflags);
		viddev->CreateVideoProcessor(enumerator, 0, &processor);
		ctx->QueryInterface(&vidcontext);
		//TODO: Initialize

		streaminfo.Enable = true;
		streaminfo.FutureFrames = 0;
		streaminfo.InputFrameOrField = 0;
		streaminfo.OutputIndex = 0;
		streaminfo.PastFrames = 0;
		


		dev->AddRef();
		MFT_REGISTER_TYPE_INFO info;
		info.guidMajorType = MFMediaType_Video;
		info.guidSubtype = MFVideoFormat_H264;
		IMFActivate** codes = 0;
		UINT32 codelen;
		//MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_HARDWARE, 0, &info, &codes, &codelen);
		MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_HARDWARE, 0, &info, &codes, &codelen);
		codes[0]->ActivateObject(__uuidof(IMFTransform), (void**)&encoder);
		for (size_t i = 0; i < codelen; i++) {
			codes[i]->Release();
		}
		CoTaskMemFree(codes);

		encoder->QueryInterface(&generator);
		IMFAttributes* monetaryfund = 0;

		encoder->GetAttributes(&monetaryfund);
		UINT32 asyncs = 0;
		monetaryfund->GetUINT32(MF_TRANSFORM_ASYNC, &asyncs);
		monetaryfund->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, true);
		monetaryfund->Release();

		IMFMediaType* o = 0;
		

		MFCreateMediaType(&o);
		o->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		o->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
		o->SetUINT32(MF_MT_AVG_BITRATE, 10000000*5); //TODO: Set this to available bandwidth on network link. Currently optimized for local transfers.
		MFSetAttributeRatio(o, MF_MT_FRAME_RATE, 30, 1);
		MFSetAttributeSize(o, MF_MT_FRAME_SIZE, 1920, 1080); //TODO: Get from texture info
		o->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_MixedInterlaceOrProgressive);
		o->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Simple);
		o->SetUINT32(CODECAPI_AVEncCommonRateControlMode, eAVEncCommonRateControlMode_LowDelayVBR);
		UINT togepi = 0;
		IMFDXGIDeviceManager* devmgr = 0;
		MFCreateDXGIDeviceManager(&togepi, &devmgr);
		devmgr->ResetDevice(dev, togepi);

		HRESULT e = encoder->GetStreamIDs(1, &thebird, 1, &isthe);
		e = encoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, ULONG_PTR(devmgr));
		e = encoder->SetOutputType(isthe,o,0);
		o->Release();
		o = 0;
		MFCreateMediaType(&o);
		o->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		o->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
		o->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_MixedInterlaceOrProgressive);
		MFSetAttributeSize(o, MF_MT_FRAME_SIZE, 1920, 1080); //TODO: Load from texture
		MFSetAttributeRatio(o, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		MFSetAttributeRatio(o, MF_MT_FRAME_RATE, 30, 1);
		e = encoder->SetInputType(thebird, o, 0);
		o->Release();
		DWORD flags;
		e = encoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
		e = encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
		e = encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
		
		encodeThread = new std::thread([=]() {
			while (running) {
				IMFMediaEvent* evt = 0;
				generator->GetEvent(0, &evt);
				MediaEventType type;
				evt->GetType(&type);
				switch (type) {
				case METransformHaveOutput:
				{
					DWORD status;
					lastFrameTime = std::chrono::steady_clock::now();
					framecount = 0;
					MFT_OUTPUT_DATA_BUFFER sample;
					memset(&sample, 0, sizeof(sample));
					sample.dwStreamID = isthe;
					encoder->ProcessOutput(0, 1, &sample, &status);
					if (sample.pSample) {
						IMFMediaBuffer* vampire = 0;
						sample.pSample->GetBufferByIndex(0, &vampire);
						DWORD maxlen = 0;
						DWORD len = 0;
						BYTE* me = 0;
						vampire->Lock(&me, &maxlen, &len);
						packetCallback(me, len);
						vampire->Unlock();
						vampire->Release();
						sample.pSample->Release();
					}
					if (sample.pEvents) {
						sample.pEvents->Release();
					}
				}
				break;
				case METransformNeedInput:
				{
					ID3D11Texture2D* vidframe = 0;
					{
						std::unique_lock<std::mutex> l(mtx);
						while (running) {
							if (pendingFrames.size()) {
								vidframe = pendingFrames.front();
								break;
							}
						}
					}
					if (!vidframe) {
						break;
					}


					//DXGI_FORMAT_NV12
					IMFMediaBuffer* buffy = 0;
					MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), vidframe, 0, false, &buffy);
					IMFSample* sample = 0;
					MFCreateSample(&sample);
					sample->AddBuffer(buffy);

					sample->SetSampleDuration(100000);
					IMFMediaEvent* mevt = 0;
					HRESULT result = encoder->ProcessInput(isthe, sample, 0);
					sample->Release();
					vidframe->Release();
					buffy->Release();
				}
				break;
				}
			}
		});


		
	}




	std::queue<ID3D11Texture2D*> pendingFrames;
	std::condition_variable evt;
	std::mutex mtx;
	bool running = true;
	std::thread* encodeThread;




	
	int framecount = 0;
	///<summary>Encodes a single frame of video</summary>
	void WriteFrame(ID3D11Texture2D* frame) {
		//TODO: Encode the video frame here.
		//Format conversion
		velociraptor:
		IMFMediaEvent* evt = 0;
		HRESULT res = generator->GetEvent(MF_EVENT_FLAG_NO_WAIT, &evt);
		
		if (!evt) {
			return; //Drop frame -- pipeline not yet ready.
		}

		MediaEventType type;
		evt->GetType(&type);
		if (frame == 0 && type == METransformNeedInput) {
			//Encoder is malfunctioning.
			goto velociraptor;
		}
		switch (type) {
		case METransformHaveOutput:
		{
			DWORD status;
			startOutput:
			lastFrameTime = std::chrono::steady_clock::now();
			framecount = 0;
			MFT_OUTPUT_DATA_BUFFER sample;
			memset(&sample, 0, sizeof(sample));
			sample.dwStreamID = isthe;
			encoder->ProcessOutput(0, 1, &sample, &status);
			if (sample.pSample) {
				IMFMediaBuffer* vampire = 0;
				sample.pSample->GetBufferByIndex(0, &vampire);
				DWORD maxlen = 0;
				DWORD len = 0;
				BYTE* me = 0;
				vampire->Lock(&me, &maxlen, &len);
				packetCallback(me, len);
				vampire->Unlock();
				vampire->Release();
				sample.pSample->Release();
			}
			if (sample.pEvents) {
				sample.pEvents->Release();
			}
			if (frame == 0) {
				//draining
				goto velociraptor;
			}
			if (sample.pSample) {
				goto startOutput;
			}
		}
			break;
		case METransformNeedInput:
		{
			//DXGI_FORMAT_NV12
			D3D11_TEXTURE2D_DESC ription;
			frame->GetDesc(&ription);
			ription.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_VIDEO_ENCODER;
			ription.Format = DXGI_FORMAT_NV12;
			ription.MiscFlags = 0;
			ID3D11Texture2D* vidframe = 0;
			dev->CreateTexture2D(&ription, 0, &vidframe);
			D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputdesc = {};
			inputdesc.FourCC = 0;
			inputdesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
			inputdesc.Texture2D.ArraySlice = 0;
			inputdesc.Texture2D.MipSlice = 0;
			D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outdesc = {};
			outdesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
			outdesc.Texture2D.MipSlice = 0;
			ID3D11VideoProcessorInputView* inputview = 0;
			ID3D11VideoProcessorOutputView* outputview = 0;
			
			viddev->CreateVideoProcessorInputView(frame, enumerator, &inputdesc, &inputview);
			viddev->CreateVideoProcessorOutputView(vidframe, enumerator, &outdesc, &outputview);
		
			streaminfo.pInputSurface = inputview;
			HRESULT blt = vidcontext->VideoProcessorBlt(processor, outputview, 0, 1, &streaminfo);
			inputview->Release();
			outputview->Release();

			IMFMediaBuffer* buffy = 0;
			MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), vidframe, 0, false, &buffy);
			IMFSample* sample = 0;
			MFCreateSample(&sample);
			sample->AddBuffer(buffy);
			
			sample->SetSampleDuration(100000);
			IMFMediaEvent* mevt = 0;
			HRESULT result = encoder->ProcessInput(isthe, sample, 0);
			framecount++;
			/*if (framecount > 1) {
				framecount = 0;
				encoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
			}*/
			sample->Release();
			vidframe->Release();
			buffy->Release();
		}
			break;
		case METransformDrainComplete: 
		{
			encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
			framecount = 0;
		}
									   break;
		}
		evt->Release();
	}
	std::chrono::steady_clock::time_point lastFrameTime;
	void Flush() {
		if (framecount) {
			encoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
			WriteFrame(0);
		}
	}
};


class VideoDecoder {
public:
	VideoDecoder() {

	}
	void SendPacket(unsigned char* packet, size_t sz) {
		//TODO: Decode the frame here.
	}
};

class WPFEngine {
public:
	IDirect3D9Ex* ctx;
	IDirect3DDevice9Ex* dev;
	IDirect3DTexture9* tex;
	IDirect3DSurface9* surface;
	IDirect3DSurface9* intermediate;
	HANDLE sharehandle;
	IDXGIOutputDuplication* dupe;
	int width;
	int height;

	ID3D11Device* dev11 = 0;
	ID3D11DeviceContext* ctx11 = 0;
	WPFEngine(HWND ow, void(*packetCallback)(unsigned char*, int)) {
		dupe = 0;
		tex = 0; //if tex && !shareHandle then segfault?????
		sharehandle = 0;
		Direct3DCreate9Ex(D3D_SDK_VERSION,&ctx);
		D3DDISPLAYMODE mode;
		ctx->GetAdapterDisplayMode(0, &mode);
		D3DPRESENT_PARAMETERS args;
		memset(&args, 0, sizeof(args));
		args.BackBufferWidth = mode.Width;
		args.BackBufferHeight = mode.Height;
		args.BackBufferFormat = mode.Format;
		width = mode.Width;
		height = mode.Height;
		args.BackBufferCount = 1;
		args.MultiSampleType = D3DMULTISAMPLE_NONE;
		args.MultiSampleQuality = 0;
		args.SwapEffect = D3DSWAPEFFECT_DISCARD;
		args.hDeviceWindow = ow;
		args.Windowed = true;
		args.EnableAutoDepthStencil = 0;
		args.Flags = 0;
		args.PresentationInterval = 0;
		HRESULT res = ctx->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, ow, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &args,0, &dev);
		dev->CreateTexture(mode.Width, mode.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex,&sharehandle);
		tex->GetSurfaceLevel(0, &surface);
		dev->CreateOffscreenPlainSurface(mode.Width, mode.Height, mode.Format, D3DPOOL_DEFAULT, &intermediate, 0);
		
		
		IDXGIDevice* dxgi = 0;
		D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, D3D11_CREATE_DEVICE_VIDEO_SUPPORT, 0, 0, D3D11_SDK_VERSION, &dev11, 0, &ctx11);
		UINT resetToken = 0;
		IMFDXGIDeviceManager* mngr = 0;
		MFCreateDXGIDeviceManager(&resetToken, &mngr);
		mngr->ResetDevice(dev11, resetToken);
		
		mngr->Release();
		dev11->QueryInterface(&dxgi);
		IDXGIAdapter* adapter;
		dxgi->GetParent(__uuidof(IDXGIAdapter), (void**)&adapter);
		IDXGIOutput* output;
		adapter->EnumOutputs(0, &output);
		IDXGIOutput1* output1 = 0;
		output->QueryInterface(&output1);
		if (output1) {
			output1->DuplicateOutput(dev11, &dupe);
			output1->Release();
		}
		output->Release();
		adapter->Release();
		dxgi->Release();
		DrawBackbuffer();

		encoder = new VideoEncoder(dev11,ctx11,packetCallback);
	}
	VideoEncoder* encoder;
	///<summary>Takes a snapshot of the desktop and encodes it into a video frame</summary>
	void RecordFrame() {
		if (dupe) {
			DXGI_OUTDUPL_FRAME_INFO desc;
			IDXGIResource* resource = 0;
			dupe->AcquireNextFrame(20, &desc, &resource);

			if (resource) {
				ID3D11Texture2D* tex = 0;
				resource->QueryInterface(&tex);
				encoder->WriteFrame(tex);
				tex->Release();
				resource->Release();
				dupe->ReleaseFrame();
			}
			else {
				encoder->Flush();

			}
		}
		else {
			HDC washington = 0;
			intermediate->GetDC(&washington);
			HDC desktop = GetDC(0);
			//TODO: BitBlt here is really slow because we're copying between GPU/system memory here.
			BOOL success = BitBlt(washington, 0, 0, width, height, desktop, 0, 0, SRCCOPY);
			ReleaseDC(0, desktop);
			intermediate->ReleaseDC(washington);
			//TODO: UpdateSurface only works for system->GPU copy
			HRESULT res = dev->StretchRect(intermediate, 0, surface, 0, D3DTEXF_NONE);
			ID3D11Resource* videoframe = 0;
			dev11->OpenSharedResource(sharehandle, __uuidof(ID3D11Resource), (void**)&videoframe);
			ID3D11Texture2D* texture = 0;
			videoframe->QueryInterface(&texture);
			encoder->WriteFrame(texture);
			texture->Release();
			videoframe->Release();
		}
	}


	///<summary>Takes a snapshot of the desktop and mirrors it in the WPF window</summary>
	void DrawBackbuffer() {
		if (dupe) {
			DXGI_OUTDUPL_FRAME_INFO desc;
			IDXGIResource* resource = 0;
			dupe->AcquireNextFrame(-1, &desc, &resource);
			
			if (resource) {
				ID3D11Texture2D* tex = 0;
				resource->QueryInterface(&tex);
				//D3D11_TEXTURE2D_DESC texdesc;
				//tex->GetDesc(&texdesc);
				ID3D11Resource* desttx = 0;
				dev11->OpenSharedResource(sharehandle, __uuidof(ID3D11Resource), (void**)&desttx);
				ctx11->CopyResource(desttx, tex);
				tex->Release();
				desttx->Release();
				resource->Release();
				dupe->ReleaseFrame();
			}
		} else {
			HDC washington = 0;
			intermediate->GetDC(&washington);
			HDC desktop = GetDC(0);
			//TODO: BitBlt here is really slow because we're copying between GPU/system memory here.
			BOOL success = BitBlt(washington, 0, 0, width, height, desktop, 0, 0, SRCCOPY);
			ReleaseDC(0, desktop);
			intermediate->ReleaseDC(washington);
			//TODO: UpdateSurface only works for system->GPU copy

			HRESULT res = dev->StretchRect(intermediate, 0, surface, 0, D3DTEXF_NONE);
		}
		
	}
	~WPFEngine() {
		ctx->Release();
		surface->Release();
		intermediate->Release();
		tex->Release();
		dev->Release();
		if (dupe) {
			dupe->Release();
		}
		if (dev11) {
			dev11->Release();
		}
		if (ctx11) {
			ctx11->Release();
		}
		delete encoder;
	}
};

typedef struct {
	IDirect3DSurface9* surface;
	WPFEngine* engine;
} ENGINE_CONTEXT;
extern "C" {

	__declspec(dllexport) void DrawBackbuffer(WPFEngine* engine) {
		engine->DrawBackbuffer();
	}

	__declspec(dllexport) void RecordFrame(WPFEngine* engine) {
		engine->RecordFrame();
	}
	__declspec(dllexport) ENGINE_CONTEXT CreateEngine(HWND ow, void(*packetCallback)(unsigned char*, int)) {
		ENGINE_CONTEXT ctx;
		ctx.engine = new WPFEngine(ow,packetCallback);
		ctx.surface = ctx.engine->surface; //Default to desktop
		return ctx;
	}

}