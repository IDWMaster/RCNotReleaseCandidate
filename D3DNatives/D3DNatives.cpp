/*
Copyright 2017 Brian Bosak
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "stdafx.h"
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <Mferror.h>
#include <mfcaptureengine.h>
#include <queue>
#include <evr.h>
#include <codecapi.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <wmcodecdsp.h>

class VideoEncoder {
public:
	IMFTransform* encoder = 0;
	IMFTransform* converter = 0;
	DWORD thebird = 0;
	DWORD isthe = 0;
	DWORD proc_in = 0;
	DWORD proc_out = 0;
	IMFMediaEventGenerator* generator = 0;
	ID3D11Device* dev = 0;
	ID3D11DeviceContext* ctx = 0;
	ID3D11VideoDevice* viddev = 0;
	ID3D11VideoProcessor* processor = 0;
	ID3D11VideoContext* vidcontext = 0;
	ID3D11VideoProcessorEnumerator* enumerator = 0;
	D3D11_VIDEO_PROCESSOR_STREAM streaminfo;
	void(*packetCallback)(int64_t,unsigned char*, int);
	VideoEncoder(ID3D11Device* dev, ID3D11DeviceContext* ctx, void(*packetCallback)(int64_t,unsigned char*, int)) :dev(dev), ctx(ctx), packetCallback(packetCallback) {
		MFStartup(MF_VERSION);
		memset(&streaminfo, 0, sizeof(streaminfo));

		dev->QueryInterface(&viddev);
		if (viddev) {
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
			enumerator->CheckVideoProcessorFormat(DXGI_FORMAT_B8G8R8A8_UNORM, &supportflags);
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

		}

		MFT_REGISTER_TYPE_INFO info;
		info.guidMajorType = MFMediaType_Video;
		info.guidSubtype = MFVideoFormat_H264;
		IMFActivate** codes = 0;
		UINT32 codelen;
		//MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, MFT_ENUM_FLAG_HARDWARE, 0, &info, &codes, &codelen);
		MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, viddev ? MFT_ENUM_FLAG_HARDWARE : MFT_ENUM_FLAG_SYNCMFT, 0, &info, &codes, &codelen);
		HRESULT e = S_OK;
		IMFDXGIDeviceManager* devmgr = 0;
		UINT togepi = 0;
		MFCreateDXGIDeviceManager(&togepi, &devmgr);
		devmgr->ResetDevice(dev, togepi);

		CoCreateInstance(CLSID_VideoProcessorMFT, 0, CLSCTX_INPROC_SERVER, IID_IMFTransform, (void**)&converter);
		{
			IMFMediaType* o = 0;
			MFCreateMediaType(&o);
			o->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			o->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
			MFSetAttributeSize(o, MF_MT_FRAME_SIZE, 1920, 1080); //TODO: Get from texture info
			HRESULT e = converter->SetInputType(0, o, 0);
			o->Release();
			o = 0;
			MFCreateMediaType(&o);
			o->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			o->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
			MFSetAttributeSize(o, MF_MT_FRAME_SIZE, 1920, 1080); //TODO: Get from texture info
			e = converter->SetOutputType(0, o, 0);
			
			e = converter->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
			o->Release();
		}
		for (size_t i = 0; i < codelen; i++) {
			codes[i]->ActivateObject(__uuidof(IMFTransform), (void**)&encoder);
			IMFAttributes* monetaryfund = 0;
			encoder->GetAttributes(&monetaryfund);
			UINT32 asyncs = 0;
			monetaryfund->GetUINT32(MF_TRANSFORM_ASYNC, &asyncs);
			monetaryfund->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, true);
			monetaryfund->SetUINT32(MF_LOW_LATENCY, true);
			monetaryfund->Release();
			e = encoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, ULONG_PTR(devmgr));
			if (e == S_OK || !viddev) {
				break;
			}
			encoder->Release();
		}
		for (size_t i = 0; i < codelen; i++) {
			codes[i]->Release();
		}
		CoTaskMemFree(codes);
		if (viddev) {
			encoder->QueryInterface(&generator);
		}

		

		IMFMediaType* o = 0;
		

		MFCreateMediaType(&o);
		o->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		o->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
		o->SetUINT32(MF_MT_AVG_BITRATE, 10000000/2); //TODO: Set this to available bandwidth on network link. Currently optimized for local transfers.
		MFSetAttributeRatio(o, MF_MT_FRAME_RATE, 60, 1);
		MFSetAttributeSize(o, MF_MT_FRAME_SIZE, 1920, 1080); //TODO: Get from texture info
		o->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		o->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);
		
		

		e = encoder->GetStreamIDs(1, &thebird, 1, &isthe);
		
		devmgr->Release();
		e = encoder->SetOutputType(isthe,o,0);
		o->Release();
		o = 0;
		MFCreateMediaType(&o);
		o->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		o->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
		o->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		MFSetAttributeSize(o, MF_MT_FRAME_SIZE, 1920, 1080); //TODO: Load from texture
		MFSetAttributeRatio(o, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		MFSetAttributeRatio(o, MF_MT_FRAME_RATE, 60, 1);
		e = encoder->SetInputType(thebird, o, 0);
		o->Release();
		DWORD flags;
		e = encoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
		e = encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
		e = encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
		//TODO: Encoder is causing memory leak.
		if (viddev) {
			encodeThread = new std::thread([=]() {
				int pendingFrameCount = 0;
				while (running) {
					IMFMediaEvent* evt = 0;
					generator->GetEvent(0, &evt);
					if (evt == 0) {
						continue;
					}
					MediaEventType type;
					evt->GetType(&type);
					switch (type) {
					case METransformHaveOutput:
					{
						pendingFrameCount--;
						if (pendingFrameCount < 0) {
							pendingFrameCount = 0;
						}
						DWORD status;
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
							LONGLONG duration; //it's a LONG LONG sample (sure have an awfully slow frame rate)!
							sample.pSample->GetSampleTime(&duration);
							packetCallback(duration, me, len);
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
						IMFSample* vidframe = 0;
						{
							std::unique_lock<std::mutex> l(mtx);
							while (running) {
								if (pendingFrames.size()) {
									vidframe = pendingFrames.front();
									pendingFrames.pop();
									break;
								}
								else {

									if (lastBuffer) {
										//Replay last buffer
										MFCreateSample(&vidframe);
										vidframe->AddBuffer(lastBuffer);
										auto now = std::chrono::steady_clock::now();
										vidframe->SetSampleDuration(std::chrono::duration_cast<std::chrono::microseconds> (now - lastFrameTime).count());
										vidframe->SetSampleTime(std::chrono::duration_cast<std::chrono::microseconds> (now - refclock).count());
										break;
									}
									else {
										this->evt.wait(l);
									}
								}
							}
						}
						if (!vidframe) {
							break;
						}

						pendingFrameCount++;
						//DXGI_FORMAT_NV12
						HRESULT result = encoder->ProcessInput(isthe, vidframe, 0);
						vidframe->Release();
					}
					break;
					case METransformDrainComplete:
						encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
						break;
					}
					evt->Release();
				}
			});
		}

	}




	std::queue<IMFSample*> pendingFrames;
	std::condition_variable evt;
	std::mutex mtx;
	bool running = true;
	std::thread* encodeThread = 0;


	IMFMediaBuffer* lastBuffer = 0;
	std::chrono::steady_clock::time_point refclock = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point lastFrameTime = std::chrono::steady_clock::now();
	///<summary>Encodes a single frame of video</summary>
	void WriteFrame(ID3D11Texture2D* frame) {
		//TODO: Encode the video frame here.
		//Format conversion
		if (viddev) {
			IMFSample* sample = 0;


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

			MFCreateSample(&sample);
			sample->AddBuffer(buffy);
			auto now = std::chrono::steady_clock::now();
			sample->SetSampleDuration(std::chrono::duration_cast<std::chrono::microseconds> (now - lastFrameTime).count());
			sample->SetSampleTime(std::chrono::duration_cast<std::chrono::microseconds> (now - refclock).count());
			lastFrameTime = now;

			std::unique_lock<std::mutex> l(mtx);
			while (pendingFrames.size() > 3) {
				IMFSample* sample = pendingFrames.front();
				pendingFrames.pop();
				sample->Release();

			}
			if (lastBuffer) {
				lastBuffer->Release();
				lastBuffer = 0;
			}
			lastBuffer = buffy;
			vidframe->Release();
			pendingFrames.push(sample);
			evt.notify_one();
		}
		else {
			//Software encode
			ID3D11Texture2D* cputex = 0;
			D3D11_TEXTURE2D_DESC ription;
			frame->GetDesc(&ription);
			ription.MiscFlags = 0;
			ription.BindFlags = 0;
			ription.Usage = D3D11_USAGE_STAGING;
			ription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			ription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			dev->CreateTexture2D(&ription, 0, &cputex);
			ctx->CopyResource(cputex, frame);
			D3D11_MAPPED_SUBRESOURCE mapped = {};
			
			IMFMediaBuffer* buffy = 0;
			MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), cputex, 0, false, &buffy);
			cputex->Release();
			IMFSample* sample = 0;
			MFCreateSample(&sample);
			sample->AddBuffer(buffy);
			buffy->Release();
			auto now = std::chrono::steady_clock::now();
			sample->SetSampleDuration(std::chrono::duration_cast<std::chrono::microseconds> (now - lastFrameTime).count());
			sample->SetSampleTime(std::chrono::duration_cast<std::chrono::microseconds> (now - refclock).count());
			lastFrameTime = now;
			HRESULT res = converter->ProcessInput(0, sample, 0);
			sample->Release();
			sample = 0;
			DWORD mac = 0; //MAC == status symbol
			MFT_OUTPUT_DATA_BUFFER slayer = {}; //MFT must provide output samples
			MFCreateSample(&sample);
			buffy = 0;
			
			MFCreate2DMediaBuffer(1920, 1080, MFVideoFormat_NV12.Data1, false, &buffy);
			sample->AddBuffer(buffy);
			buffy->Release();
			slayer.pSample = sample;
			res = converter->ProcessOutput(0, 1, &slayer, &mac);
			//TODO: Memory leak below!
			res = encoder->ProcessInput(0, sample, 0);
			mac = 0;
			sample->Release();
			sample = 0;
			MFT_OUTPUT_STREAM_INFO outinfo;
			encoder->GetOutputStreamInfo(0, &outinfo);
			buffy = 0;
			slayer = {};
			MFCreateMemoryBuffer(outinfo.cbSize, &buffy);
			MFCreateSample(&sample);
			sample->AddBuffer(buffy);
			slayer.pSample = sample;
			res = encoder->ProcessOutput(0, 1, &slayer, &mac);
			if (res == S_OK) {
				int64_t timestamp = 0;
				sample->GetSampleTime(&timestamp); //TODO: Time is always 0 here.
				unsigned char* data = 0;
				DWORD maxlen = 0;
				DWORD len = 0;
				buffy->Lock(&data, &maxlen, &len);
				packetCallback(timestamp, data, len);
				buffy->Unlock();
			}
			buffy->Release();
			sample->Release();
		
		}
	}
	


	~VideoEncoder() {
		running = false;
		evt.notify_all();
		if (converter) {
			converter->Release();
		}
		if (this->encoder) {
			encoder->Release();
			IMFShutdown* shutdown = 0;
			if (shutdown) {
				encoder->QueryInterface(&shutdown);
				shutdown->Shutdown();
				shutdown->Release();
			}
		}
		if (this->encodeThread) {
			encodeThread->join();
			delete encodeThread;
		}
		if (this->enumerator) {
			enumerator->Release();
		}
		if (this->generator) {
			generator->Release();
		}
		{
			std::unique_lock<std::mutex> l(mtx);
			while (pendingFrames.size()) {
				IMFSample* sample = pendingFrames.front();
				pendingFrames.pop();
				sample->Release();
			}
		}
		if (this->processor) {
			processor->Release();
		}
		if (this->vidcontext) {
			vidcontext->Release();
		}
		if (this->viddev) {
			viddev->Release();
		}
	}
};


class VideoDecoder {
public:
	IMFTransform* encoder = 0;
	DWORD thebird = 0;
	DWORD isthe = 0;
	DWORD proc_in = 0;
	DWORD proc_out = 0;
	ID3D11Device* dev = 0;
	ID3D11DeviceContext* ctx = 0;
	ID3D11VideoDevice* viddev = 0;
	ID3D11VideoProcessor* processor = 0;
	ID3D11VideoContext* vidcontext = 0;
	ID3D11VideoProcessorEnumerator* enumerator = 0;
	D3D11_VIDEO_PROCESSOR_STREAM streaminfo;
	ID3D11VideoProcessorOutputView* outview;
	ID3D11Texture2D* rawframe = 0;
	void(*packetCallback)(int64_t, unsigned char*, int);
	VideoDecoder(ID3D11Device* dev, ID3D11DeviceContext* ctx,ID3D11Texture2D* rawframe,  void(*packetCallback)(int64_t, unsigned char*, int)) :dev(dev), ctx(ctx), packetCallback(packetCallback),rawframe(rawframe) {
		rawframe->AddRef();
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
		viddev->CreateVideoProcessor(enumerator, 0, &processor);
		ctx->QueryInterface(&vidcontext);
		D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outdesc;
		outdesc.Texture2D.MipSlice = 0;
		outdesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
		viddev->CreateVideoProcessorOutputView(rawframe, enumerator, &outdesc, &outview);
		//TODO: Initialize

		streaminfo.Enable = true;
		streaminfo.FutureFrames = 0;
		streaminfo.InputFrameOrField = 0;
		streaminfo.OutputIndex = 0;
		streaminfo.PastFrames = 0;



		MFT_REGISTER_TYPE_INFO info;
		info.guidMajorType = MFMediaType_Video;
		info.guidSubtype = MFVideoFormat_H264;
		IMFActivate** codes = 0;
		UINT32 codelen;
		MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SYNCMFT, &info, 0, &codes, &codelen);
		codes[0]->ActivateObject(__uuidof(IMFTransform), (void**)&encoder);
		for (size_t i = 0; i < codelen; i++) {
			codes[i]->Release();
		}
		CoTaskMemFree(codes);

		IMFAttributes* monetaryfund = 0;

		encoder->GetAttributes(&monetaryfund);
		UINT32 asyncs = 0;
		//monetaryfund->GetUINT32(MF_TRANSFORM_ASYNC, &asyncs);
		monetaryfund->SetUINT32(MF_LOW_LATENCY, true);
		HRESULT sv = monetaryfund->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, 1);
		monetaryfund->Release();

		IMFMediaType* o = 0;



		MFCreateMediaType(&o);
		o->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		o->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
		o->SetUINT32(MF_MT_AVG_BITRATE, 10000000 / 2); //TODO: Set this to available bandwidth on network link. Currently optimized for local transfers.
		MFSetAttributeRatio(o, MF_MT_FRAME_RATE, 60, 1);
		MFSetAttributeSize(o, MF_MT_FRAME_SIZE, 1920, 1080); //TODO: Get from texture info
		o->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		o->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);
		UINT togepi = 0;
		IMFDXGIDeviceManager* devmgr = 0;
		MFCreateDXGIDeviceManager(&togepi, &devmgr);
		devmgr->ResetDevice(dev, togepi);

		HRESULT e = encoder->GetStreamIDs(1, &thebird, 1, &isthe);
		e = encoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, ULONG_PTR(devmgr));
		devmgr->Release();
		
		e = encoder->SetInputType(thebird, o, 0);
		o->Release();
		MFCreateMediaType(&o);
		o->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		o->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
		o->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		MFSetAttributeSize(o, MF_MT_FRAME_SIZE, 1920, 1080); //TODO: Load from texture
		MFSetAttributeRatio(o, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		MFSetAttributeRatio(o, MF_MT_FRAME_RATE, 60, 1);
		e = encoder->SetOutputType(isthe, o, 0);
		o->Release();
		
		o = 0;

		e = encoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
		e = encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
		e = encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

		

	}


	void SendPacket(unsigned char* data, int base) {
		IMFSample* sample = 0;
		MFCreateSample(&sample);
		IMFMediaBuffer* buffer = 0;
		MFCreateMemoryBuffer(base, &buffer);
		buffer->SetCurrentLength(base);
		DWORD maxlen;
		DWORD curlen;
		unsigned char* db = 0;
		buffer->Lock(&db, &maxlen, &curlen);
		memcpy(db, data, base);
		buffer->Unlock();
		sample->AddBuffer(buffer);
		buffer->Release();
		HRESULT er = encoder->ProcessInput(thebird, sample, 0);
		sample->Release();
		DWORD status = 0;
		MFT_OUTPUT_DATA_BUFFER yourinforatreat = {};
		yourinforatreat.dwStreamID = isthe;
		buffer = 0;
		sample = 0;
		yourinforatreat.pSample = 0;
		er = encoder->ProcessOutput(0, 1, &yourinforatreat, &status);
		if (er == MF_E_TRANSFORM_STREAM_CHANGE) {
			IMFMediaType* o = 0;
			HRESULT e = encoder->GetStreamIDs(1, &thebird, 1, &isthe);
			encoder->GetOutputAvailableType(0, 0, &o);
			e = encoder->SetOutputType(0, o, 0);
			
			o->Release();
		}
		if (yourinforatreat.pSample) {
			IMFDXGIBuffer* buffy = 0;
			buffer = 0;
			yourinforatreat.pSample->GetBufferByIndex(0, &buffer);
			buffer->QueryInterface(&buffy);
			ID3D11Texture2D* texture;
			buffy->GetResource(__uuidof(ID3D11Texture2D), (void**)&texture);
			D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC indesc;
			indesc.FourCC = 0;
			indesc.Texture2D.ArraySlice = 0;
			indesc.Texture2D.MipSlice = 0;
			indesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
			ID3D11VideoProcessorInputView* inview = 0;
			viddev->CreateVideoProcessorInputView(texture, enumerator, &indesc, &inview);
			streaminfo.pInputSurface = inview;

			vidcontext->VideoProcessorBlt(processor, outview, 0, 1, &streaminfo);
			packetCallback(0, 0, 0);
			inview->Release();
			buffer->Release();
			buffy->Release();
			texture->Release();
			yourinforatreat.pSample->Release();
		}
	}

	std::queue<IMFSample*> pendingFrames;
	std::condition_variable evt;
	std::mutex mtx;
	bool running = true;
	std::thread* encodeThread = 0;


	IMFMediaBuffer* lastBuffer = 0;
	std::chrono::steady_clock::time_point refclock = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point lastFrameTime = std::chrono::steady_clock::now();
	


	~VideoDecoder() {
		running = false;
		evt.notify_all();
		
		if (this->encodeThread) {
			encodeThread->join();
			delete encodeThread;
		}
		if (this->enumerator) {
			enumerator->Release();
		}
		{
			std::unique_lock<std::mutex> l(mtx);
			while (pendingFrames.size()) {
				IMFSample* sample = pendingFrames.front();
				pendingFrames.pop();
				sample->Release();
			}
		}
		if (this->processor) {
			processor->Release();
		}
		if (this->vidcontext) {
			vidcontext->Release();
		}
		if (this->viddev) {
			viddev->Release();
		}
		rawframe->Release();
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
	void(*packetCallback)(int64_t, unsigned char*, int);
	WPFEngine(HWND ow, void(*packetCallback)(int64_t, unsigned char*, int)):packetCallback(packetCallback) {
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
		//TODO: Memory leak caused in constructor of VideoEncoder (even with no frames being encoded)
		encoder = new VideoEncoder(dev11,ctx11,packetCallback);


		ID3D11Resource* videoframe = 0;
		dev11->OpenSharedResource(sharehandle, __uuidof(ID3D11Resource), (void**)&videoframe);
		ID3D11Texture2D* texture = 0;
		videoframe->QueryInterface(&texture);
		videoframe->Release();
		

		decoder = new VideoDecoder(dev11,ctx11, texture,packetCallback);
		texture->Release();
	}


	void SendPacket(unsigned char* packet, int size)
	{
		decoder->SendPacket(packet, size);
	}
	VideoDecoder* decoder = 0;
	VideoEncoder* encoder = 0;
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
				dupe->ReleaseFrame(); //TODO: This deadlocks with ProccessInput (causing internal driver deadlock due to pipeline state dependency)
				printf("released");
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
		delete decoder;

		ULONG refcount; 
		if (dupe) {
			refcount = dupe->Release();
		}
		if (dev11) {
			dev11->Release();
		}
		
		if (encoder) {
			delete encoder;
		}
		if (ctx11) {
			refcount = ctx11->Release();
		}
	}
};

typedef struct {
	IDirect3DSurface9* surface;
	WPFEngine* engine;
} ENGINE_CONTEXT;

class Injector {
public:
	BOOL s;
	Injector() {
		s = InitializeTouchInjection(32, TOUCH_FEEDBACK_DEFAULT);
	}
	void Touch(int op, int x, int y, int point) {
		POINTER_TOUCH_INFO info = {};
		info.orientation = 0;
		info.pointerInfo.pointerType = PT_TOUCH;
		info.pointerInfo.pointerId = point;
		info.pointerInfo.pointerFlags = POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT;
		info.touchMask = 0;
		switch (op) {
		case 1:
			info.pointerInfo.pointerFlags = POINTER_FLAG_UP;
			break;
		case 2:
			info.pointerInfo.pointerFlags &= ~POINTER_FLAG_DOWN;
			info.pointerInfo.pointerFlags |= POINTER_FLAG_UPDATE;
			break;
		}
		info.pointerInfo.ptPixelLocation.x = x;
		info.pointerInfo.ptPixelLocation.y = y;
		s = InjectTouchInput(1, &info);
		
		s = false;
	}
};

static Injector touchInjector;

extern "C" {
	__declspec(dllexport) void SendPacket(WPFEngine* engine, unsigned char* packet, int len) {
		engine->SendPacket(packet, len);
	}
	__declspec(dllexport) void DrawBackbuffer(WPFEngine* engine) {
		engine->DrawBackbuffer();
	}

	__declspec(dllexport) void RecordFrame(WPFEngine* engine) {
		engine->RecordFrame();
	}
	__declspec(dllexport) ENGINE_CONTEXT CreateEngine(HWND ow, void(*packetCallback)(int64_t,unsigned char*, int)) {
		ENGINE_CONTEXT ctx;
		ctx.engine = new WPFEngine(ow,packetCallback);
		ctx.surface = ctx.engine->surface; //Default to desktop
		return ctx;
	}

	__declspec(dllexport) void DispatchInput(int type, int x, int y, int id) {
		if (type > 2) {
			SendMessageW(0, WM_LBUTTONDOWN, 0, 0);
		}
		else {
			touchInjector.Touch(type, x, y, id);
		}
	}
	__declspec(dllexport) void FreeEngine(WPFEngine* engine) {
		delete engine;
	}
}