#include "StaticMeshComponent.h"

#include "ECWorld/World.h"
#include "Log/Log.h"
#include "Math/MeshGenerator.h"
#include "Rendering/Utility/VertexLayoutUtility.h"
#include "Scene/VertexFormat.h"

#include <bgfx/bgfx.h>

#include <optional>

namespace engine
{

void StaticMeshComponent::Reset()
{
	m_pMeshData = nullptr;
	m_pRequiredVertexFormat = nullptr;

	m_vertexBuffer.clear();
	m_vertexBufferHandle = UINT16_MAX;

	m_indexBuffer.clear();
	m_indexBufferHandle = UINT16_MAX;

	// Debug
	m_aabb.Clear();
	m_aabbVertexBuffer.clear();
	m_aabbVBH = UINT16_MAX;

	m_aabbIndexBuffer.clear();
	m_aabbIBH = UINT16_MAX;
}

void StaticMeshComponent::BuildDebug()
{
	m_aabb = m_pMeshData->GetAABB();
	if (m_aabb.IsEmpty())
	{
		return;
	}

	cd::VertexFormat vertexFormat;
	vertexFormat.AddAttributeLayout(cd::VertexAttributeType::Position, cd::AttributeValueType::Float, 3);

	//vertexFormat.AddAttributeLayout(cd::VertexAttributeType::Color, cd::AttributeValueType::Float, 4);
	std::optional<cd::Mesh> optMesh = cd::MeshGenerator::Generate(cd::Box(m_aabb.Min(), m_aabb.Max()), vertexFormat);
	if (!optMesh.has_value())
	{
		return;
	}

	const cd::Mesh& meshData = optMesh.value();
	const uint32_t vertexCount = meshData.GetVertexCount();
	m_aabbVertexBuffer.resize(vertexCount * vertexFormat.GetStride());
	uint32_t currentDataSize = 0U;
	auto currentDataPtr = m_aabbVertexBuffer.data();
	for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
	{
		// position
		const cd::Point& position = meshData.GetVertexPosition(vertexIndex);
		constexpr uint32_t posDataSize = cd::Point::Size * sizeof(cd::Point::ValueType);
		std::memcpy(&currentDataPtr[currentDataSize], position.Begin(), posDataSize);
		currentDataSize += posDataSize;

		// barycentric
		//const cd::Vec4f& barycentricCoordinates = meshData.GetVertexColor(0U, vertexIndex);
		//constexpr uint32_t bcDataSize = cd::Vec4f::Size * sizeof(cd::Vec4f::ValueType);
		//std::memcpy(&currentDataPtr[currentDataSize], barycentricCoordinates.Begin(), bcDataSize);
		//currentDataSize += bcDataSize;
	}
	
	// AABB should always use u16 index type.
	size_t indexTypeSize = sizeof(uint16_t);
	m_aabbIndexBuffer.resize(12 * 2 * indexTypeSize);
	assert(meshData.GetPolygonCount() <= static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()));
	currentDataSize = 0U;
	currentDataPtr = m_aabbIndexBuffer.data();

	std::vector<uint16_t> indexes =
	{
		0U,1U,1U,5U,5U,4U,4U,0U,
		0U,2U,1U,3U,5U,7U,4U,6U,
		2U,3U,3U,7U,7U,6U,6U,2U
	};

	for (const auto& index : indexes)
	{
		std::memcpy(&currentDataPtr[currentDataSize], &index, indexTypeSize);
		currentDataSize += static_cast<uint32_t>(indexTypeSize);
	}

	bgfx::VertexLayout vertexLayout;
	VertexLayoutUtility::CreateVertexLayout(vertexLayout, vertexFormat.GetVertexLayout());
	m_aabbVBH = bgfx::createVertexBuffer(bgfx::makeRef(m_aabbVertexBuffer.data(), static_cast<uint32_t>(m_aabbVertexBuffer.size())), vertexLayout).idx;
	m_aabbIBH = bgfx::createIndexBuffer(bgfx::makeRef(m_aabbIndexBuffer.data(), static_cast<uint32_t>(m_aabbIndexBuffer.size())), 0U).idx;
}

void StaticMeshComponent::Build()
{
	CD_ASSERT(m_pMeshData && m_pRequiredVertexFormat, "Input data is not ready.");

	if (!m_pMeshData->GetVertexFormat().IsCompatiableTo(*m_pRequiredVertexFormat))
	{
		CD_ERROR("Current mesh data is not compatiable to required vertex format.");
		return;
	}

	const bool containsPosition = m_pRequiredVertexFormat->Contains(cd::VertexAttributeType::Position);
	const bool containsNormal = m_pRequiredVertexFormat->Contains(cd::VertexAttributeType::Normal);
	const bool containsTangent = m_pRequiredVertexFormat->Contains(cd::VertexAttributeType::Tangent);
	const bool containsBiTangent = m_pRequiredVertexFormat->Contains(cd::VertexAttributeType::Bitangent);
	const bool containsUV = m_pRequiredVertexFormat->Contains(cd::VertexAttributeType::UV);
	const bool containsColor = m_pRequiredVertexFormat->Contains(cd::VertexAttributeType::Color);

	// TODO : Store animation here temporarily to test.
	const bool containsBoneIndex = m_pRequiredVertexFormat->Contains(cd::VertexAttributeType::BoneIndex);
	const bool containsBoneWeight = m_pRequiredVertexFormat->Contains(cd::VertexAttributeType::BoneWeight);

	const uint32_t vertexCount = m_pMeshData->GetVertexCount();
	const uint32_t vertexFormatStride = m_pRequiredVertexFormat->GetStride();

	m_vertexBuffer.resize(vertexCount * vertexFormatStride);

	uint32_t currentDataSize = 0U;
	auto currentDataPtr = m_vertexBuffer.data();

	auto FillVertexBuffer = [&currentDataPtr, &currentDataSize](const void* pData, uint32_t dataSize)
	{
		std::memcpy(&currentDataPtr[currentDataSize], pData, dataSize);
		currentDataSize += dataSize;
	};

	for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
	{
		if (containsPosition)
		{
			constexpr uint32_t dataSize = cd::Point::Size * sizeof(cd::Point::ValueType);
			FillVertexBuffer(m_pMeshData->GetVertexPosition(vertexIndex).Begin(), dataSize);
		}

		if (containsNormal)
		{
			constexpr uint32_t dataSize = cd::Direction::Size * sizeof(cd::Direction::ValueType);
			FillVertexBuffer(m_pMeshData->GetVertexNormal(vertexIndex).Begin(), dataSize);
		}

		if (containsTangent)
		{
			constexpr uint32_t dataSize = cd::Direction::Size * sizeof(cd::Direction::ValueType);
			FillVertexBuffer(m_pMeshData->GetVertexTangent(vertexIndex).Begin(), dataSize);
		}
		
		if (containsBiTangent)
		{
			constexpr uint32_t dataSize = cd::Direction::Size * sizeof(cd::Direction::ValueType);
			FillVertexBuffer(m_pMeshData->GetVertexBiTangent(vertexIndex).Begin(), dataSize);
		}
		
		if (containsUV)
		{
			constexpr uint32_t dataSize = cd::UV::Size * sizeof(cd::UV::ValueType);
			FillVertexBuffer(m_pMeshData->GetVertexUV(0)[vertexIndex].Begin(), dataSize);
		}

		if (containsColor)
		{
			constexpr uint32_t dataSize = cd::Color::Size * sizeof(cd::Color::ValueType);
			FillVertexBuffer(m_pMeshData->GetVertexColor(0)[vertexIndex].Begin(), dataSize);
		}

		if (containsBoneIndex && containsBoneWeight)
		{
			std::vector<uint16_t> vertexBoneIDs;
			std::vector<cd::VertexWeight> vertexBoneWeights;

			for(uint32_t vertexBoneIndex = 0U; vertexBoneIndex < 4; ++vertexBoneIndex)
			{
				cd::BoneID boneID;
				if (vertexBoneIndex < m_pMeshData->GetVertexInfluenceCount())
				{
					boneID = m_pMeshData->GetVertexBoneID(vertexBoneIndex, vertexIndex);
				}

				if (boneID.IsValid())
				{
					vertexBoneIDs.push_back(static_cast<uint16_t>(boneID.Data()));
					vertexBoneWeights.push_back(m_pMeshData->GetVertexWeight(vertexBoneIndex, vertexIndex));
				}
				else
				{
					vertexBoneIDs.push_back(127);
					vertexBoneWeights.push_back(0.0f);
				}
			}

			// TODO : Change storage to a TVector<uint16_t, InfluenceCount> and TVector<float, InfluenceCount> ?
			FillVertexBuffer(vertexBoneIDs.data(), static_cast<uint32_t>(vertexBoneIDs.size() * sizeof(uint16_t)));
			FillVertexBuffer(vertexBoneWeights.data(), static_cast<uint32_t>(vertexBoneWeights.size() * sizeof(cd::VertexWeight)));
		}
	}

	// Create vertex buffer.
	bgfx::VertexLayout vertexLayout;
	VertexLayoutUtility::CreateVertexLayout(vertexLayout, m_pRequiredVertexFormat->GetVertexLayout());
	const bgfx::Memory* pVertexBufferRef = bgfx::makeRef(m_vertexBuffer.data(), static_cast<uint32_t>(m_vertexBuffer.size()));
	bgfx::VertexBufferHandle vertexBufferHandle = bgfx::createVertexBuffer(pVertexBufferRef, vertexLayout);
	assert(bgfx::isValid(vertexBufferHandle));
	m_vertexBufferHandle = vertexBufferHandle.idx;

	// Fill index buffer data.
	bool useU16Index = vertexCount <= static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()) + 1U;
	uint32_t indexTypeSize = useU16Index ? sizeof(uint16_t) : sizeof(uint32_t);
	m_indexBuffer.resize(m_pMeshData->GetPolygonCount() * 3 * indexTypeSize);

	currentDataSize = 0U;
	currentDataPtr = m_indexBuffer.data();
	auto FillIndexBuffer = [&currentDataPtr, &currentDataSize](const void* pData, uint32_t dataSize)
	{
		std::memcpy(&currentDataPtr[currentDataSize], pData, dataSize);
		currentDataSize += dataSize;
	};

	for (const auto& polygon : m_pMeshData->GetPolygons())
	{
		if (useU16Index)
		{
			// cd::Mesh always uses uint32_t to store index so it is not convenient to copy servals elements at the same time.
			for (auto vertexID : polygon)
			{
				uint16_t vertexIndex = static_cast<uint16_t>(vertexID.Data());
				FillIndexBuffer(&vertexIndex, indexTypeSize);
			}
		}
		else
		{
			FillIndexBuffer(polygon.data(), static_cast<uint32_t>(polygon.size() * indexTypeSize));
		}
	}

	// Create index buffer.
	const bgfx::Memory* pIndexBufferRef = bgfx::makeRef(m_indexBuffer.data(), static_cast<uint32_t>(m_indexBuffer.size()));
	bgfx::IndexBufferHandle indexBufferHandle = bgfx::createIndexBuffer(pIndexBufferRef, useU16Index ? 0U : BGFX_BUFFER_INDEX32);
	assert(bgfx::isValid(indexBufferHandle));
	m_indexBufferHandle = indexBufferHandle.idx;

	// Build debug data.
	BuildDebug();
}

}