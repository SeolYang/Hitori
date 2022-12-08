#include <Core.h>
#include <AssetContext.h>

namespace sy
{
	bool AssetContext::SaveBinary(const std::string_view path, const Asset& asset)
	{
		std::ofstream outFileStream;
		outFileStream.open(path, std::ios::binary | std::ios::out);

		outFileStream.write(asset.TypeIdentifier, LengthOfArray(asset.TypeIdentifier));

		const auto version = asset.Version;
		outFileStream.write(reinterpret_cast<const char*>(&version), sizeof(asset.Version));

		const auto metadataSize = asset.Metadata.size();
		outFileStream.write(reinterpret_cast<const char*>(&metadataSize), sizeof(metadataSize));

		const auto blobSize = asset.Blob.size();
		outFileStream.write(reinterpret_cast<const char*>(&blobSize), sizeof(blobSize));

		outFileStream.close();

		return true;
	}

	std::optional<AssetContext::Asset> AssetContext::LoadBinary(const std::string_view path)
	{
		std::ifstream inFileStream;
		inFileStream.open(path, std::ios::binary);
		if (!inFileStream.is_open())
		{
			spdlog::warn("Failed to load asset file from {}.", path);
			return std::nullopt;
		}

		inFileStream.seekg(0);

		Asset asset;
		inFileStream.read(asset.TypeIdentifier, LengthOfArray(asset.TypeIdentifier));
		inFileStream.read(reinterpret_cast<char*>(&asset.Version), sizeof(asset.Version));

		auto metadataSize = asset.Metadata.size();
		inFileStream.read(reinterpret_cast<char*>(&metadataSize), sizeof(metadataSize));

		auto blobSize = asset.Blob.size();
		inFileStream.read(reinterpret_cast<char*>(&blobSize), sizeof(blobSize));

		asset.Metadata.resize(metadataSize);
		asset.Blob.resize(blobSize);

		inFileStream.read(asset.Metadata.data(), metadataSize);
		inFileStream.read(asset.Blob.data(), blobSize);

		return asset;
	}
}
