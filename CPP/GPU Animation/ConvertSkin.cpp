//----------------------------------------------------------------------------
// Copyright 2026, Ed Keenan, all rights reserved.
//----------------------------------------------------------------------------
#include "GLTF.h"
#include "meshData.h"
#include "meshDataConverter.h"
#include "json.hpp"
#include "File.h"
#include "MathEngine.h"

#include "ConvertSkin.h"

using namespace tinygltf;
using json = nlohmann::json;

namespace Azul
{
	void ConvertSkin(const char* const pFileName, const char* const pTargetName)
	{
		bool status;
		tinygltf::Model gltfModel;

		Trace::out("%-25s", pFileName);

		// Strip the extension .glb off the name
		unsigned int len = (uint32_t)strlen(pFileName);
		char* pTmp = new char[len + 1]();
		memset(pTmp, 0, len);
		memcpy(pTmp, pFileName, len - strlen(".glb"));

		// base name
		std::string BaseName = pTargetName;
		delete[] pTmp;

		// Load the gltfModel
		status = GLTF::Load(gltfModel, pFileName);
		assert(status);

		// Read glb into memory (raw buffer)
		char* poBuff = nullptr;
		unsigned int BuffSize(0);

		status = GLTF::GetGLBRawBuffer(poBuff, BuffSize, pFileName);
		assert(status);

		// Get GLB_Header
		GLB_header glbHeader;
		status = GLTF::GetGLBHeader(glbHeader, poBuff, BuffSize);
		assert(status);

		// Get Raw JSON
		char* poJSON = nullptr;
		unsigned int JsonSize(0);
		status = GLTF::GetRawJSON(poJSON, JsonSize, poBuff, BuffSize);
		assert(status);

		// Get the Binary Buffer Address
		char* pBinaryBuff = nullptr;
		unsigned int BinaryBuffSize = 0;
		status = GLTF::GetBinaryBuffPtr(pBinaryBuff, BinaryBuffSize, poBuff, BuffSize);
		assert(status);

		// --------------------------------------------------
		//  Iterate through all meshes and combine them
		// --------------------------------------------------

		unsigned char* pBuff = (unsigned char*)&gltfModel.buffers[0].data[0];

		// Combined data vectors
		std::vector<Vec4ui> combinedJoints;
		std::vector<Vec4f> combinedWeights;
		std::vector<Vec3f> combinedPos;
		std::vector<Vec3f> combinedNorms;
		std::vector<Vec2f> combinedTexCoord;
		std::vector<Vec3ui> combinedIndex;

		// Inverse bind matrices (shared across all meshes)
		std::vector<Mat4> invBone;

		// Process inverse bind matrices once (shared by all meshes)
		size_t inverseMatrixAccessorIndex = (size_t)gltfModel.skins[0].inverseBindMatrices;
		auto InverseAccessor = gltfModel.accessors[inverseMatrixAccessorIndex];
		auto InverseBuffView = gltfModel.bufferViews[(size_t)InverseAccessor.bufferView];
		unsigned char* pInverseMatrixBuff = pBuff + InverseBuffView.byteOffset;
		assert(InverseAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
		assert(InverseAccessor.type == TINYGLTF_TYPE_MAT4);

		Mat4* pMat4 = (Mat4*)pInverseMatrixBuff;
		for (size_t i = 0; i < InverseAccessor.count; i++)
		{
			assert(pMat4);
			invBone.push_back(pMat4[i]);
		}

		// Track vertex offset for index remapping
		unsigned int vertexOffset = 0;

		// Iterate through all meshes
		for (size_t meshIdx = 0; meshIdx < gltfModel.meshes.size(); meshIdx++)
		{
			// Iterate through all primitives in the mesh
			for (size_t primIdx = 0; primIdx < gltfModel.meshes[meshIdx].primitives.size(); primIdx++)
			{
				auto& primitive = gltfModel.meshes[meshIdx].primitives[primIdx];

				// Check if this primitive has skinning data
				auto weightIt = primitive.attributes.find("WEIGHTS_0");
				auto jointIt = primitive.attributes.find("JOINTS_0");

				if (weightIt == primitive.attributes.end() || jointIt == primitive.attributes.end())
				{
					Trace::out("\nWarning: Mesh[%d] Primitive[%d] has no skinning data, skipping...\n", (int)meshIdx, (int)primIdx);
					continue;
				}

				// Get accessor indices for this primitive
				size_t weightAccessorIndex = (size_t)weightIt->second;
				size_t jointAccessorIndex = (size_t)jointIt->second;
				size_t indexAccessorIndex = (size_t)primitive.indices;
				size_t posAccessorIndex = (size_t)primitive.attributes.find("POSITION")->second;
				size_t normsAccessorIndex = (size_t)primitive.attributes.find("NORMAL")->second;
				size_t texCoordAccessorIndex = (size_t)primitive.attributes.find("TEXCOORD_0")->second;

				// Joint
				auto JointAccessor = gltfModel.accessors[jointAccessorIndex];
				auto JointBuffView = gltfModel.bufferViews[(size_t)JointAccessor.bufferView];
				unsigned char* pJointBuff = pBuff + JointBuffView.byteOffset;
				assert(JointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
				assert(JointAccessor.type == TINYGLTF_TYPE_VEC4);

				// Weight
				auto WeightAccessor = gltfModel.accessors[weightAccessorIndex];
				auto WeightBuffView = gltfModel.bufferViews[(size_t)WeightAccessor.bufferView];
				unsigned char* pWeightBuff = pBuff + WeightBuffView.byteOffset;
				assert(WeightAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				assert(WeightAccessor.type == TINYGLTF_TYPE_VEC4);

				// Verify counts match
				assert(WeightAccessor.count == JointAccessor.count);

				// Index
				auto IndexAccessor = gltfModel.accessors[indexAccessorIndex];
				auto IndexBuffView = gltfModel.bufferViews[(size_t)IndexAccessor.bufferView];
				unsigned char* pIndexBuff = pBuff + IndexBuffView.byteOffset;
				assert(IndexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
				assert(IndexAccessor.type == TINYGLTF_TYPE_SCALAR);

				// Pos
				auto PosAccessor = gltfModel.accessors[posAccessorIndex];
				auto PosBuffView = gltfModel.bufferViews[(size_t)PosAccessor.bufferView];
				unsigned char* pPosBuff = pBuff + PosBuffView.byteOffset;
				assert(PosAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				assert(PosAccessor.type == TINYGLTF_TYPE_VEC3);

				// Norms
				auto NormsAccessor = gltfModel.accessors[normsAccessorIndex];
				auto NormsBuffView = gltfModel.bufferViews[(size_t)NormsAccessor.bufferView];
				unsigned char* pNormsBuff = pBuff + NormsBuffView.byteOffset;
				assert(NormsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				assert(NormsAccessor.type == TINYGLTF_TYPE_VEC3);

				// Tex Coord
				auto TexCoordAccessor = gltfModel.accessors[texCoordAccessorIndex];
				auto TexCoordBuffView = gltfModel.bufferViews[(size_t)TexCoordAccessor.bufferView];
				unsigned char* pTexCoordBuff = pBuff + TexCoordBuffView.byteOffset;
				assert(TexCoordAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				assert(TexCoordAccessor.type == TINYGLTF_TYPE_VEC2);

				// Verify all vertex attributes have same count
				assert(PosAccessor.count == NormsAccessor.count);
				assert(PosAccessor.count == TexCoordAccessor.count);
				assert(PosAccessor.count == JointAccessor.count);
				assert(PosAccessor.count == WeightAccessor.count);

				// Get pointers to data
				Vec4si* pJointVect4 = (Vec4si*)pJointBuff;
				Vec4f* pWeightVect4 = (Vec4f*)pWeightBuff;
				Vec3f* pPos = (Vec3f*)pPosBuff;
				Vec3f* pNorms = (Vec3f*)pNormsBuff;
				Vec2f* pTexCoord = (Vec2f*)pTexCoordBuff;

				// Append vertex data
				for (size_t i = 0; i < PosAccessor.count; i++)
				{
					combinedJoints.push_back(Vec4ui((unsigned int)pJointVect4[i].x,
						(unsigned int)pJointVect4[i].y,
						(unsigned int)pJointVect4[i].z,
						(unsigned int)pJointVect4[i].w));
					combinedWeights.push_back(pWeightVect4[i]);
					combinedPos.push_back(pPos[i]);
					combinedNorms.push_back(pNorms[i]);
					combinedTexCoord.push_back(pTexCoord[i]);
				}

				// Append index data with offset
				Vec3si* pVec3us = (Vec3si*)pIndexBuff;
				for (size_t i = 0; i < IndexAccessor.count / 3; i++)
				{
					combinedIndex.push_back(Vec3ui(pVec3us[i].x + vertexOffset,
						pVec3us[i].y + vertexOffset,
						pVec3us[i].z + vertexOffset));
				}

				// Update vertex offset for next mesh
				vertexOffset += (unsigned int)PosAccessor.count;
			}
		}

		// --------------------------------------------------
		//  Fill Mesh Data protoBuff with combined data
		// --------------------------------------------------

		meshData runModel;

		// Model Name
		std::string meshName = BaseName;
		memcpy_s(runModel.pMeshName, meshData::FILE_NAME_SIZE, meshName.c_str(), meshName.size());

		// Set vbo with combined data
		GLTF::SetCustomVBO(runModel.vbo_vert,
			(char*)&combinedPos[0],
			(uint32_t)combinedPos.size() * sizeof(Vec3f),
			(uint32_t)combinedPos.size(),
			vboData::VBO_COMPONENT::FLOAT,
			vboData::VBO_TYPE::VEC3,
			vboData::VBO_TARGET::ARRAY_BUFFER
		);

		GLTF::SetCustomVBO(runModel.vbo_norm,
			(char*)&combinedNorms[0],
			(uint32_t)combinedNorms.size() * sizeof(Vec3f),
			(uint32_t)combinedNorms.size(),
			vboData::VBO_COMPONENT::FLOAT,
			vboData::VBO_TYPE::VEC3,
			vboData::VBO_TARGET::ARRAY_BUFFER
		);

		GLTF::SetCustomVBO(runModel.vbo_uv,
			(char*)&combinedTexCoord[0],
			(uint32_t)combinedTexCoord.size() * sizeof(Vec2f),
			(uint32_t)combinedTexCoord.size(),
			vboData::VBO_COMPONENT::FLOAT,
			vboData::VBO_TYPE::VEC2,
			vboData::VBO_TARGET::ARRAY_BUFFER
		);

		GLTF::SetCustomVBO_index(runModel.vbo_index,
			(char*)&combinedIndex[0],
			(uint32_t)combinedIndex.size() * sizeof(Vec3ui),
			(uint32_t)combinedIndex.size() * 3,
			vboData::VBO_COMPONENT::UNSIGNED_INT,
			vboData::VBO_TYPE::SCALAR,
			vboData::VBO_TARGET::ELEMENT_ARRAY_BUFFER
		);

		GLTF::SetCustomVBO(runModel.vbo_weights,
			(char*)&combinedWeights[0],
			(uint32_t)combinedWeights.size() * sizeof(Vec4f),
			(uint32_t)combinedWeights.size(),
			vboData::VBO_COMPONENT::FLOAT,
			vboData::VBO_TYPE::VEC4,
			vboData::VBO_TARGET::ARRAY_BUFFER
		);

		GLTF::SetCustomVBO(runModel.vbo_joints,
			(char*)&combinedJoints[0],
			(uint32_t)combinedJoints.size() * sizeof(Vec4ui),
			(uint32_t)combinedJoints.size(),
			vboData::VBO_COMPONENT::UNSIGNED_INT,
			vboData::VBO_TYPE::VEC4,
			vboData::VBO_TARGET::ARRAY_BUFFER
		);

		GLTF::SetCustomVBO(runModel.vbo_invBind,
			(char*)&invBone[0],
			(uint32_t)invBone.size() * sizeof(Mat4),
			(uint32_t)invBone.size(),
			vboData::VBO_COMPONENT::FLOAT,
			vboData::VBO_TYPE::MAT4,
			vboData::VBO_TARGET::ARRAY_BUFFER
		);

		// PolyCount
		runModel.triCount = runModel.vbo_index.count / 3;
		runModel.vertCount = runModel.vbo_vert.count;

		// RenderMode
		runModel.mode = meshDataConverter::GetMode(gltfModel.meshes[0].primitives[0].mode);
		assert(runModel.mode == meshData::RENDER_MODE::MODE_TRIANGLES);

		meshData_proto mA_proto;
		runModel.Serialize(mA_proto);

		// -------------------------------
		//  Write to file
		//--------------------------------

		File::Handle fh;
		File::Error err;

		const char* pProtoFileType = nullptr;
		status = GLTF::GetAzulProtoType(pProtoFileType, runModel);
		assert(status);

		std::string OutputFileName = BaseName + pProtoFileType + ".proto.azul";
		Trace::out(" --> %+30s\n", OutputFileName.c_str());

		err = File::Open(fh, OutputFileName.c_str(), File::Mode::WRITE);
		assert(err == File::Error::SUCCESS);

		std::string strOut;
		mA_proto.SerializeToString(&strOut);

		File::Write(fh, strOut.data(), (uint32_t)strOut.length());
		assert(err == File::Error::SUCCESS);

		err = File::Close(fh);
		assert(err == File::Error::SUCCESS);

		// -------------------------------
		// Read and recreate model data
		// -------------------------------
		unsigned char* poNewBuff;
		unsigned int newBuffSize;

		err = File::GetFileAsBuffer(OutputFileName.c_str(), poNewBuff, newBuffSize);
		assert(err == File::Error::SUCCESS);

		std::string strIn((const char*)poNewBuff, newBuffSize);
		meshData_proto mB_proto;

		mB_proto.ParseFromString(strIn);

		meshData mB;
		mB.Deserialize(mB_proto);

		delete[] poJSON;
		delete[] poBuff;
		delete[] poNewBuff;
	}
}

