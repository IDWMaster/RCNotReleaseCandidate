// D3DNatives.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfcaptureengine.h>



class VideoEncoder {
public:
	VideoEncoder() {

	}
	///<summary>Encodes a single frame of video</summary>
	void WriteFrame(ID3D11Texture2D* frame) {
		//TODO: Encode the video frame here.
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
	WPFEngine(HWND ow) {
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

		encoder = new VideoEncoder();
	}
	VideoEncoder* encoder;
	///<summary>Takes a snapshot of the desktop and encodes it into a video frame</summary>
	void RecordFrame() {
		if (dupe) {
			DXGI_OUTDUPL_FRAME_INFO desc;
			IDXGIResource* resource = 0;
			dupe->AcquireNextFrame(-1, &desc, &resource);

			if (resource) {
				ID3D11Texture2D* tex = 0;
				resource->QueryInterface(&tex);
				encoder->WriteFrame(tex);
				tex->Release();
				resource->Release();
				dupe->ReleaseFrame();
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
	__declspec(dllexport) ENGINE_CONTEXT CreateEngine(HWND ow) {
		ENGINE_CONTEXT ctx;
		ctx.engine = new WPFEngine(ow);
		ctx.surface = ctx.engine->surface; //Default to desktop
		return ctx;
	}

}