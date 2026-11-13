#include "engine/Image.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <limits>
#include <stdexcept>
#include <string>

namespace BlackLight::Engine
{
	namespace
	{
		/// Initializes COM for the current thread when it has not already been initialized.
		class ComApartment final
		{
		public:
			/// Acquires a multithreaded COM apartment or accepts an existing apartment.
			ComApartment()
			{
				const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
				owns_initialization_ = SUCCEEDED(result);
				if (FAILED(result) && result != RPC_E_CHANGED_MODE)
				{
					throw std::runtime_error("COM initialization failed while decoding an image");
				}
			}
			/// Releases only the COM initialization acquired by this object.
			~ComApartment()
			{
				if (owns_initialization_)
					CoUninitialize();
			}

		private:
			bool owns_initialization_ = false;
		};

		/// Converts a cached byte stream to tightly packed RGBA8 pixels.
		DecodedImage DecodeBytes(const std::vector<std::uint8_t> &bytes)
		{
			if (bytes.empty() || bytes.size() > std::numeric_limits<DWORD>::max())
				throw std::runtime_error("Image asset is empty or exceeds WIC stream limits");
			ComApartment apartment;
			Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
			HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
											  IID_PPV_ARGS(factory.GetAddressOf()));
			if (FAILED(result))
				throw std::runtime_error("Windows Imaging Component factory creation failed");
			Microsoft::WRL::ComPtr<IWICStream> stream;
			result = factory->CreateStream(stream.GetAddressOf());
			if (FAILED(result) ||
				FAILED(stream->InitializeFromMemory(
					const_cast<BYTE *>(bytes.data()), static_cast<DWORD>(bytes.size()))))
				throw std::runtime_error("Windows Imaging Component could not read the image stream");
			Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
			result = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad,
													  decoder.GetAddressOf());
			if (FAILED(result))
				throw std::runtime_error("Image format is unsupported or the image data is invalid");
			Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
			if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())))
				throw std::runtime_error("Image frame decoding failed");
			UINT width = 0;
			UINT height = 0;
			if (FAILED(frame->GetSize(&width, &height)) || !width || !height ||
				width > std::numeric_limits<UINT>::max() / 4u ||
				height > std::numeric_limits<UINT>::max() / (width * 4u) ||
				static_cast<std::size_t>(width) * height >
					std::numeric_limits<std::size_t>::max() / 4u)
				throw std::runtime_error("Image dimensions are empty or too large");
			Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
			if (FAILED(factory->CreateFormatConverter(converter.GetAddressOf())) ||
				FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
											 WICBitmapDitherTypeNone, nullptr, 0.0,
											 WICBitmapPaletteTypeCustom)))
				throw std::runtime_error("Image could not be converted to RGBA8");
			DecodedImage image;
			image.width = width;
			image.height = height;
			image.rgba.resize(static_cast<std::size_t>(width) * height * 4u);
			if (FAILED(converter->CopyPixels(nullptr, width * 4u,
											 static_cast<UINT>(image.rgba.size()), image.rgba.data())))
				throw std::runtime_error("Decoded image pixels could not be copied");
			return image;
		}
	}

	/// Loads cached binary content and decodes its first frame through WIC.
	DecodedImage DecodeImage(ContentManager &content, const std::filesystem::path &path)
	{
		return DecodeBytes(*content.LoadBinary(path));
	}

	/// Creates a linearly filtered RGBA8 texture from a decoded image.
	Rendering::Texture LoadTexture(ContentManager &content, Rendering::RenderingDevice &device,
								   const std::filesystem::path &path, Rendering::TextureFilter filter)
	{
		const DecodedImage image = DecodeImage(content, path);
		Rendering::TextureDesc description;
		description.width = image.width;
		description.height = image.height;
		description.format = Rendering::ResourceFormat::RGBA8Unorm;
		description.initial_data = image.rgba.data();
		description.initial_data_size = image.rgba.size();
		description.minification_filter = filter;
		description.magnification_filter = filter;
		return device.CreateTexture(description);
	}
}
