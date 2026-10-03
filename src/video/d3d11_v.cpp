/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file d3d11_v.cpp Implementation of the Windows (Direct3D 11) video driver. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../error_func.h"
#include "../os/windows/win32.h"
#include "../blitter/factory.hpp"
#include "../core/geometry_func.hpp"
#include "../window_func.h"
#include "../framerate_type.h"
#include "../library_loader.h"
#include "../table/hlsl_shader.h"
#include "win32_v.h"
#include <windows.h>
#include <objbase.h>
#include <wrl/client.h>
#include <dxgi1_5.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "../safeguards.h"

extern bool _window_maximize; ///< Declared in win32_v.cpp.

using Microsoft::WRL::ComPtr;

/**
 * Structure for an HLSL constant buffer containing the sprite position, screen size, zoom level, and flags for RGB and crash mode.
 */
struct HlslConstantBuffer {
	FLOAT sprite[4]; ///< Sprite position.
	FLOAT screen[2]; ///< Screen size.
	FLOAT zoom;      ///< Zoom level.
	BOOL rgb;        ///< Flag indicating whether RGB mode is enabled.
	BOOL crash;      ///< Flag indicating whether crash mode is enabled.
	FLOAT pad[3];    ///< Padding.
};

static LibraryLoader _d3d11("d3d11.dll"); ///< LibraryLoader instance for the D3D11 DLL.
static std::unique_ptr<LibraryLoader> _d3dcompiler; ///< Pointer to a LibraryLoader instance for the D3DCompiler DLL.

static PFN_D3D11_CREATE_DEVICE _D3D11CreateDevice; ///< Pointer to the D3D11CreateDevice function.
static pD3DCompile _D3DCompile; ///< Pointer to the D3DCompile function.

/**
 * Compiles an HLSL shader from source code.
 *
 * @param source The HLSL source code.
 * @param func The entry point function name.
 * @param profile The shader profile (e.g., "vs_4_0" for vertex shader, "ps_4_0" for pixel shader).
 * @param code A reference to a ComPtr that will hold the compiled shader object.
 * @return An error message if compilation fails, or an empty string if successful.
 */
static std::string_view CompileShader(std::string_view source, std::string_view func, std::string_view profile, ComPtr<ID3DBlob> &code)
{
	ComPtr<ID3DBlob> error;

	HRESULT result = _D3DCompile(source.data(), source.length(),
		nullptr, nullptr, nullptr,
		func.data(), profile.data(), D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &error);

	if (FAILED(result)) {
		if (error) {
			return (char *)error->GetBufferPointer();
		}

		return "Shader compile error";
	}

	return {};
}

/**
 * Creates a vertex shader from HLSL source code.
 *
 * @param device The D3D11 device.
 * @param source The HLSL source code.
 * @param shader A reference to a ComPtr that will hold the created vertex shader.
 * @return An error message if creation fails, or an empty string if successful.
 */
static std::string_view CreateVertexShader(ID3D11Device *device, std::string_view source, ComPtr<ID3D11VertexShader> &shader)
{
	ComPtr<ID3DBlob> code;

	std::string_view msg = CompileShader(source, "vs_main", "vs_4_0", code);
	if (!msg.empty())
		return msg;

	HRESULT result = device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader);

	if (FAILED(result)) {
		return "Failed to create vertex shader";
	}

	return {};
}

/**
 * Creates a pixel shader from HLSL source code.
 *
 * @param device The D3D11 device.
 * @param source The HLSL source code.
 * @param shader A reference to a ComPtr that will hold the created pixel shader.
 * @return An error message if creation fails, or an empty string if successful.
 */
static std::string_view CreatePixelShader(ID3D11Device *device, std::string_view source, ComPtr<ID3D11PixelShader> &shader)
{
	ComPtr<ID3DBlob> code;

	std::string_view msg = CompileShader(source, "ps_main", "ps_4_0", code);
	if (!msg.empty())
		return msg;

	HRESULT result = device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader);

	if (FAILED(result)) {
		return "Failed to create pixel shader";
	}

	return {};
}

/** The D3D11 video driver for windows. */
class VideoDriver_Win32D3D11 : public VideoDriver_Win32Base {
public:
	/**
	 * Constructor for the D3D11 video driver.
	 */
	VideoDriver_Win32D3D11() : VideoDriver_Win32Base(true), dxgi_fullscreen(false), dxgi_flags(0), anim_buffer(nullptr), driver_info(this->GetName())
	{
	}

	std::optional<std::string_view> Start(const StringList &param) override;

	void Stop() override;

	bool ChangeResolution(int w, int h) override;

	bool ToggleFullscreen(bool fullscreen) override;

	bool AfterBlitterChange() override;

	bool HasEfficient8Bpp() const override
	{
		return true;
	}

	bool UseSystemCursor() override
	{
		return false;
	}

	bool HasAnimBuffer() override
	{
		return true;
	}
	uint8_t *GetAnimBuffer() override
	{
		return this->anim_buffer;
	}

	/**
	 * Get the pitch of the animation buffer.
	 * @return The pitch of the animation buffer.
	 */
	int GetAnimBufferPitch() override
	{
		return this->anim_pitch;
	}

	std::string_view GetName() const override
	{
		return "win32-d3d11";
	}
	std::string_view GetInfoString() const override
	{
		return this->driver_info;
	}

protected:
	ComPtr<IDXGISwapChain> swap_chain;            ///< DXGI swap chain.
	ComPtr<ID3D11RenderTargetView> rendertarget;  ///< D3D11 render target view for the back buffer.

	bool dxgi_force_resize_buffer = false;        ///< Flag indicating whether to force resize the swap chain buffer.
	bool dxgi_fullscreen;                         ///< Flag indicating whether we're in fullscreen mode.
	UINT dxgi_flags;                              ///< DXGI swap chain flags.

	std::string driver_info; ///< Information string about selected driver.

	uint8_t *anim_buffer = nullptr; ///< Animation buffer from D3D11 back-end.
	int anim_pitch = 0; ///< Pitch of the animation buffer from D3D11 back-end.

	bool MinimiseRestoreWithToggleFullscreen() override
	{
		return true;
	}
	uint8_t GetFullscreenBpp() override
	{
		return 32;
	} // D3D11 is always 32 bpp.

	void Paint() override;

	bool AllocateBackingStore(int w, int h, bool force = false) override;
	void *GetVideoPointer() override;
	void ReleaseVideoPointer() override;
	void PaletteChanged(HWND) override
	{
	}

	std::string_view CreateSwapchain();
	void DestroySwapchain();

private:
	ComPtr<ID3D11Device> device;                       ///< D3D11 device.
	ComPtr<ID3D11DeviceContext> device_ctx;            ///< D3D11 device context.

	ComPtr<ID3D11VertexShader> vertex_shader;          ///< Vertex shader for sprite rendering.
	ComPtr<ID3D11PixelShader> direct_shader;           ///< Pixel shader for direct rendering.
	ComPtr<ID3D11PixelShader> palette_shader;          ///< Pixel shader for palette animation.
	ComPtr<ID3D11PixelShader> rgb_mask_blend_shader;   ///< Pixel shader for RGB mask blending.
	ComPtr<ID3D11PixelShader> sprite_blend_shader;     ///< Pixel shader for sprite blending.

	ComPtr<ID3D11Buffer> constant_buffer;              ///< Constant buffer.

	ComPtr<ID3D11Texture2D> vid_texture;               ///< Video texture for the back buffer.
	ComPtr<ID3D11Texture2D> vid_texture_staging;       ///< Staging texture for video texture.
	ComPtr<ID3D11ShaderResourceView> vid_texture_srv;  ///< Shader resource view for video texture.

	ComPtr<ID3D11Texture2D> anim_texture;              ///< Animation texture for palette animation.
	ComPtr<ID3D11Texture2D> anim_texture_staging;      ///< Staging texture for animation texture.
	ComPtr<ID3D11ShaderResourceView> anim_texture_srv; ///< Shader resource view for animation texture.

	ComPtr<ID3D11Texture1D> pal_texture;               ///< Palette texture for palette animation.
	ComPtr<ID3D11ShaderResourceView> pal_texture_srv;  ///< Shader resource view for palette texture.

	ComPtr<ID3D11SamplerState> texture_sampler;        ///< Sampler state for texture sampling.

	std::string_view InitD3D();
	std::string_view Resize(int w, int h);
};

/** The factory for Windows' D3D11 video driver. */
class FVideoDriver_Win32D3D11 : public DriverFactoryBase {
public:
	/** Constructor for the D3D11 video driver factory. */
	FVideoDriver_Win32D3D11() : DriverFactoryBase(Driver::Type::Video, 10, "win32-d3d11", "Win32 D3D11 Video Driver")
	{
	}
	std::unique_ptr<Driver> CreateInstance() const override
	{
		return std::make_unique<VideoDriver_Win32D3D11>();
	}

protected:
	bool UsesHardwareAcceleration() const override
	{
		return true;
	}
};

static FVideoDriver_Win32D3D11 iFVideoDriver_Win32D3D11; ///< Factory instance for the D3D11 video driver.

/**
 * Check for the needed D3D11 functionality and allocate all resources.
 * @return Error string or nullptr if successful.
 */
std::string_view VideoDriver_Win32D3D11::InitD3D()
{
	D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_10_0;
	UINT device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
	//device_flags |= D3D11_CREATE_DEVICE_DEBUG; /* Uncomment to create device with debug layer. */

	HRESULT result = _D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		device_flags, &feature_level, 1, D3D11_SDK_VERSION,
		&this->device, nullptr, &this->device_ctx);

	if (FAILED(result))
		return "Failed to create D3D11 device";

	std::string_view err;

	err = CreateVertexShader(this->device.Get(), _vertex_shader_sprite_hlsl, this->vertex_shader);
	if (!err.empty()) return err;

	err = CreatePixelShader(this->device.Get(), _frag_shader_direct_hlsl, this->direct_shader);
	if (!err.empty()) return err;

	err = CreatePixelShader(this->device.Get(), _frag_shader_palette_hlsl, this->palette_shader);
	if (!err.empty()) return err;

	err = CreatePixelShader(this->device.Get(), _frag_shader_rgb_mask_blend_hlsl, this->rgb_mask_blend_shader);
	if (!err.empty()) return err;

	err = CreatePixelShader(this->device.Get(), _frag_shader_sprite_blend_hlsl, this->sprite_blend_shader);
	if (!err.empty()) return err;

	D3D11_BUFFER_DESC cbuffer_desc =
	{
		.ByteWidth = sizeof(HlslConstantBuffer),
		.Usage = D3D11_USAGE_IMMUTABLE,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
		.StructureByteStride = 0
	};

	HlslConstantBuffer constant =
	{
		.sprite = { 0.0f, 0.0f, 1.0f, 1.0f },
		.screen = { 1.0f, 1.0f },
		.zoom = 0.0f,
		.rgb = TRUE,
		.crash = FALSE,
	};

	D3D11_SUBRESOURCE_DATA data =
	{
		.pSysMem = &constant,
		.SysMemPitch = sizeof(constant),
		.SysMemSlicePitch = 0
	};

	result = device->CreateBuffer(&cbuffer_desc, &data, &this->constant_buffer);
	if (FAILED(result)) return "Failed to constant buffer";

	D3D11_TEXTURE1D_DESC desc =
	{
		.Width = 256,
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = DXGI_FORMAT_B8G8R8A8_UNORM,
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_SHADER_RESOURCE,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
		.MiscFlags = 0
	};

	result = device->CreateTexture1D(&desc, nullptr, &this->pal_texture);
	if (FAILED(result)) return "Failed to create texture";

	result = device->CreateShaderResourceView(this->pal_texture.Get(), nullptr, &this->pal_texture_srv);
	if (FAILED(result)) return "Failed to resource view";

	D3D11_SAMPLER_DESC sampler_desc =
	{
		.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
		.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
		.ComparisonFunc = D3D11_COMPARISON_NEVER,
		.MaxLOD = D3D11_FLOAT32_MAX
	};

	result = device->CreateSamplerState(&sampler_desc, &texture_sampler);
	if (FAILED(result)) return "Failed to create sampler state";

	return {};
}

/**
 * Resizes the D3D11 textures to the given width and height.
 *
 * @param w The new width.
 * @param h The new height.
 * @return Error string or nullptr if successful.
 */
std::string_view VideoDriver_Win32D3D11::Resize(int w, int h)
{
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();

	D3D11_TEXTURE2D_DESC desc =
	{
		.Width = (UINT) w,
		.Height = (UINT) h,
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = (bpp == 8) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM,
		.SampleDesc = { .Count = 1, .Quality = 0 },
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_SHADER_RESOURCE,
		.CPUAccessFlags = 0,
		.MiscFlags = 0
	};

	HRESULT result = device->CreateTexture2D(&desc, nullptr, &this->vid_texture);
	if (FAILED(result)) return "Failed to create texture";

	desc.BindFlags = 0;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

	result = device->CreateTexture2D(&desc, nullptr, &this->vid_texture_staging);
	if (FAILED(result)) return "Failed to create texture";

	desc.Format = DXGI_FORMAT_R8_UNORM;

	/* Does this blitter need a separate animation buffer? */
	if (BlitterFactory::GetCurrentBlitter()->NeedsAnimationBuffer()) {
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.CPUAccessFlags = 0;

		result = device->CreateTexture2D(&desc, nullptr, &this->anim_texture);
		if (FAILED(result)) return "Failed to create texture";

		desc.BindFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

		result = device->CreateTexture2D(&desc, nullptr, &this->anim_texture_staging);
		if (FAILED(result)) return "Failed to create texture";
	} else {
		/* Allocate dummy texture that always reads as 0 == no remap. */
		desc.Width = 1;
		desc.Height = 1;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.CPUAccessFlags = 0;

		byte dummy = 0;
		D3D11_SUBRESOURCE_DATA data =
		{
			.pSysMem = &dummy,
			.SysMemPitch = 1,
			.SysMemSlicePitch = 0
		};

		result = device->CreateTexture2D(&desc, &data, &this->anim_texture);
		if (FAILED(result)) return "Failed to create texture";

		this->anim_texture_staging.Reset();
	}

	result = device->CreateShaderResourceView(this->vid_texture.Get(), nullptr, &this->vid_texture_srv);
	if (FAILED(result)) return "Failed to create resource view";

	result = device->CreateShaderResourceView(this->anim_texture.Get(), nullptr, &this->anim_texture_srv);
	if (FAILED(result)) return "Failed to create resource view";

	/* Set new viewport. */
	_screen.height = h;
	_screen.width = w;
	_screen.dst_ptr = nullptr;

	return {};
}

std::optional<std::string_view> VideoDriver_Win32D3D11::Start(const StringList& param)
{
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 0) return "Only real blitters supported";

	if (_d3d11.HasError()) return "Failed to load d3d11 library";

	_D3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)_d3d11.GetFunction("D3D11CreateDevice");
	if (_d3d11.HasError()) return "Failed to import D3D11CreateDevice function";

	for (int ver = D3D_COMPILER_VERSION; ver >= 40; ver--) {
		std::string dllname = fmt::format("d3dcompiler_{}.dll", ver);
		_d3dcompiler = make_unique<LibraryLoader>(dllname);
		if (!_d3dcompiler->HasError()) break;
	}

	if (_d3dcompiler->HasError()) return "Failed to load d3dcompiler library";

	_D3DCompile = (pD3DCompile)_d3dcompiler->GetFunction("D3DCompile");
	if (_d3dcompiler->HasError()) return "Failed to import D3DCompile function";

	std::string_view err = InitD3D();

	if (!err.empty()) {
		return err;
	}

	this->Initialize();

	/* Don't use legacy MakeWindow fullscreen handling, handle this later in ToggleFullscreen.
	 * Set dxgi_fullscreen there as a hint to CreateSwapchain to allow it to select optimal DXGI swap effect. */
	bool fs = this->dxgi_fullscreen = _fullscreen;
	this->MakeWindow(false);

	this->ClientSizeChanged(this->width, this->height, true);

	if (_screen.dst_ptr == nullptr) return "D3D11 setup failed";

	this->driver_info = GetName();
	this->driver_info += " (";
	static char driver_name_utf8[1024];

	ComPtr<IDXGIDevice> dxgiDevice;
	device.As(&dxgiDevice);

	ComPtr<IDXGIAdapter> adapter;

	if (SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf()))) {
		DXGI_ADAPTER_DESC desc = {};
		adapter->GetDesc(&desc);

		this->driver_info += convert_from_fs(desc.Description, driver_name_utf8);
	}
	else {
		this->driver_info += "unknown device";
	}

	this->driver_info += ")";

	if (fs) this->ToggleFullscreen(true);

	/* Main loop expects to start with the buffer unmapped. */
	this->ReleaseVideoPointer();

	MarkWholeScreenDirty();

	this->is_game_threaded = !GetDriverParamBool(param, "no_threads") && !GetDriverParamBool(param, "no_thread");

	return std::nullopt;
}

void VideoDriver_Win32D3D11::Stop()
{
	this->DestroySwapchain();
	_D3D11CreateDevice = nullptr;
	_D3DCompile = nullptr;
	this->VideoDriver_Win32Base::Stop();
}

/**
 * Creates a DXGI swap chain for the current window.
 *
 * @return Error string or nullptr if successful.
 */
std::string_view VideoDriver_Win32D3D11::CreateSwapchain()
{
	ComPtr<IDXGIDevice> dxgi_device;
	HRESULT result = device.As(&dxgi_device);
	if (FAILED(result)) return "Failed to get DXGI device";

	ComPtr<IDXGIAdapter> dxgi_adapter;
	result = dxgi_device->GetAdapter(&dxgi_adapter);
	if (FAILED(result)) return "Failed to get DXGI adapter";

	ComPtr<IDXGIFactory> dxgi_factory;
	result = dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));
	if (FAILED(result)) return "Failed to get DXGI factory";

	ComPtr<IDXGIFactory2> dxgi_factory2;
	dxgi_factory.As(&dxgi_factory2);

	ComPtr<IDXGIFactory4> dxgi_factory4;
	dxgi_factory.As(&dxgi_factory4);

	ComPtr<IDXGIFactory5> dxgi_factory5;
	dxgi_factory.As(&dxgi_factory5);

	BOOL allow_tearing = FALSE;
	if (dxgi_factory5) {
		dxgi_factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
	}

	DXGI_SWAP_EFFECT swap_effect;

	if (dxgi_factory4) {
		/* DXGI_SWAP_EFFECT_FLIP_DISCARD supported since Windows 10/DXGI 1.4 */
		swap_effect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	}
	else if (dxgi_factory2) {
		/* DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL supported since Windows 8/DXGI 1.2 */
		swap_effect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	}
	else {
		swap_effect = DXGI_SWAP_EFFECT_DISCARD;
		allow_tearing = FALSE;
	}

	if (!allow_tearing && !this->dxgi_fullscreen && !_video_vsync) {
		/* If non-fullscreen window without vsync is requested and DXGI_FEATURE_PRESENT_ALLOW_TEARING is not present
		 * always use bitblt model, otherwise window might be always vsynced.  */
		swap_effect = DXGI_SWAP_EFFECT_DISCARD;
	}

	/* Add DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH here to allow changing fullscreen resolution. */
	this->dxgi_flags = (allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

	this->dxgi_fullscreen = false;

	DXGI_SWAP_CHAIN_DESC swap_desc = { 0 };

	swap_desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swap_desc.SampleDesc.Count = 1;
	swap_desc.SampleDesc.Quality = 0;
	swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_desc.BufferCount = 2;
	swap_desc.OutputWindow = this->main_wnd;
	swap_desc.Windowed = TRUE; /* Documentation recommends to always create windowed swapchain and only set it to fullscreen later. */
	swap_desc.SwapEffect = swap_effect;
	swap_desc.Flags = this->dxgi_flags;
	swap_desc.SwapEffect = swap_effect;

	result = dxgi_factory->CreateSwapChain(device.Get(), &swap_desc, &this->swap_chain);
	if (FAILED(result)) return "Failed to create swap chain";

	/* We don't want DXGI to intercept Alt-enter and switch to fullscreen mode by itself. */
	result = dxgi_factory->MakeWindowAssociation(this->main_wnd, DXGI_MWA_NO_ALT_ENTER);
	if (FAILED(result)) return "Failed to set DXGI window settings";

	return {};
}

/**
 * Destroys the DXGI swap chain and releases all associated resources.
 */
void VideoDriver_Win32D3D11::DestroySwapchain()
{
	this->rendertarget.Reset();
	if (this->swap_chain && this->dxgi_fullscreen) this->swap_chain->SetFullscreenState(false, nullptr);
	this->swap_chain.Reset();
}

bool VideoDriver_Win32D3D11::ChangeResolution(int w, int h)
{
	if (_window_maximize) ShowWindow(this->main_wnd, SW_SHOWNORMAL);

	DXGI_MODE_DESC mode = { 0 };
	mode.Width = w;
	mode.Height = h;
	mode.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

	HRESULT result = this->swap_chain->ResizeTarget(&mode);

	return SUCCEEDED(result);
}

bool VideoDriver_Win32D3D11::ToggleFullscreen(bool full_screen)
{
	HRESULT result = this->swap_chain->SetFullscreenState(full_screen, nullptr);
	if (FAILED(result))
		return false;

	_fullscreen = full_screen;
	this->fullscreen = full_screen;
	this->dxgi_fullscreen = full_screen;
	this->dxgi_force_resize_buffer = true;
	InvalidateWindowClassesData(WindowClass::GameOptions, 3);

	return true;
}

bool VideoDriver_Win32D3D11::AfterBlitterChange()
{
	assert(BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 0);
	this->ClientSizeChanged(this->width, this->height, true);
	return true;
}

bool VideoDriver_Win32D3D11::AllocateBackingStore(int w, int h, bool force)
{
	if (!force && w == _screen.width && h == _screen.height) return false;

	this->width = w = std::max(w, 64);
	this->height = h = std::max(h, 64);

	if (_screen.dst_ptr != nullptr) this->ReleaseVideoPointer();

	this->dirty_rect = {};
	std::string_view err = Resize(w, h);
	if (!err.empty()) {
		Debug(Facility::Driver, Severity::Critical, "%s", err);
		return false;
	}

	this->rendertarget.Reset();

	if (!this->swap_chain) {
		err = this->CreateSwapchain();
		if (!err.empty()) {
			Debug(Facility::Driver, Severity::Critical, "%s", err);
			return false;
		}
	}
	else {
		HRESULT result = this->swap_chain->ResizeBuffers(2, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, this->dxgi_flags);
		if (FAILED(result)) {
			Debug(Facility::Driver, Severity::Critical, "Failed to resize buffers");
			return false;
		}
	}

	this->dxgi_force_resize_buffer = false;

	ComPtr<ID3D11Texture2D> framebuffer;
	HRESULT result = this->swap_chain->GetBuffer(0, IID_PPV_ARGS(&framebuffer));
	if (FAILED(result)) {
		Debug(Facility::Driver, Severity::Critical, "Failed to get framebuffer");
		return false;
	}

	result = device->CreateRenderTargetView(framebuffer.Get(), nullptr, &this->rendertarget);
	if (FAILED(result)) {
		Debug(Facility::Driver, Severity::Critical, "Failed to create render target view");
		return false;
	}

	_screen.dst_ptr = this->GetVideoPointer();

	return true;
}

void* VideoDriver_Win32D3D11::GetVideoPointer()
{
	if (BlitterFactory::GetCurrentBlitter()->NeedsAnimationBuffer()) {
		D3D11_MAPPED_SUBRESOURCE mapped;
		device_ctx->Map(anim_texture_staging.Get(), 0, D3D11_MAP_READ_WRITE, 0, &mapped);
		this->anim_buffer = (uint8_t *)mapped.pData;
		this->anim_pitch = mapped.RowPitch;
	}

	D3D11_MAPPED_SUBRESOURCE mapped;
	device_ctx->Map(vid_texture_staging.Get(), 0, D3D11_MAP_READ_WRITE, 0, &mapped);

	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	_screen.pitch = mapped.RowPitch / bpp * 8;

	return mapped.pData;
}

void VideoDriver_Win32D3D11::ReleaseVideoPointer()
{
	if (this->anim_buffer != nullptr) {
		device_ctx->Unmap(anim_texture_staging.Get(), 0);

		if (!IsEmptyRect(this->dirty_rect)) {
			D3D11_BOX box =
			{
				.left = (UINT) this->dirty_rect.left,
				.top = (UINT) this->dirty_rect.top,
				.front = 0,
				.right = (UINT) this->dirty_rect.right,
				.bottom = (UINT) this->dirty_rect.bottom,
				.back = 1
			};

			device_ctx->CopySubresourceRegion(anim_texture.Get(), 0, this->dirty_rect.left, this->dirty_rect.top, 0, anim_texture_staging.Get(), 0, &box);
		}
	}

	device_ctx->Unmap(vid_texture_staging.Get(), 0);

	if (!IsEmptyRect(this->dirty_rect)) {
		D3D11_BOX box =
		{
			box.left = this->dirty_rect.left,
			box.top = this->dirty_rect.top,
			box.front = 0,
			box.right = this->dirty_rect.right,
			box.bottom = this->dirty_rect.bottom,
			box.back = 1
		};

		device_ctx->CopySubresourceRegion(vid_texture.Get(), 0, this->dirty_rect.left, this->dirty_rect.top, 0, vid_texture_staging.Get(), 0, &box);
	}

	this->dirty_rect = {};
	_screen.dst_ptr = nullptr;
	this->anim_buffer = nullptr;
	this->anim_pitch = 0;
}

void VideoDriver_Win32D3D11::Paint()
{
	PerformanceMeasurer framerate(PerformanceElement::Video);

	if (this->dxgi_force_resize_buffer) {
		this->AllocateBackingStore(this->width, this->height, true);
		MarkWholeScreenDirty();
	}

	if (local_palette.count_dirty != 0) {
		Blitter* blitter = BlitterFactory::GetCurrentBlitter();

		/* Always push a changed palette to D3D11. */
		assert(local_palette.first_dirty + local_palette.count_dirty <= 256);

		D3D11_BOX box =
		{
			.left = (UINT) local_palette.first_dirty,
			.top = 0,
			.front = 0,
			.right = (UINT) (local_palette.first_dirty + local_palette.count_dirty),
			.bottom = 1,
			.back = 1
		};

		device_ctx->UpdateSubresource(pal_texture.Get(), 0, &box, local_palette.palette + local_palette.first_dirty, 256, 0);

		if (blitter->UsePaletteAnimation() == Blitter::PaletteAnimation::Blitter) {
			blitter->PaletteAnimate(local_palette);
		}

		local_palette.count_dirty = 0;
	}

	FLOAT background_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	device_ctx->ClearRenderTargetView(rendertarget.Get(), background_color);
	device_ctx->OMSetRenderTargets(1, rendertarget.GetAddressOf(), nullptr);

	device_ctx->VSSetShader(vertex_shader.Get(), nullptr, 0);
	device_ctx->VSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
	device_ctx->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());

	ID3D11ShaderResourceView *resources[4] = { nullptr };
	resources[0] = this->vid_texture_srv.Get();
	resources[1] = this->pal_texture_srv.Get();
	if (BlitterFactory::GetCurrentBlitter()->NeedsAnimationBuffer()) {
		resources[2] = this->anim_texture_srv.Get();
		device_ctx->PSSetShader(this->rgb_mask_blend_shader.Get(), nullptr, 0);
	} else {
		if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 8) {
			device_ctx->PSSetShader(this->palette_shader.Get(), nullptr, 0);
		} else {
			device_ctx->PSSetShader(this->direct_shader.Get(), nullptr, 0);
		}
	}

	device_ctx->PSSetSamplers(0, 1, texture_sampler.GetAddressOf());
	device_ctx->PSSetShaderResources(0, 4, resources);

	D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (float)_screen.width, (float)_screen.height, 0.0f, 1.0f };
	device_ctx->RSSetViewports(1, &viewport);

	device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	device_ctx->Draw(4, 0);

	device_ctx->ClearState();

	UINT swap_flags = (!_video_vsync && !this->dxgi_fullscreen && (this->dxgi_flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)) ? DXGI_PRESENT_ALLOW_TEARING : 0;
	this->swap_chain->Present(_video_vsync ? 1 : 0, swap_flags);
}
