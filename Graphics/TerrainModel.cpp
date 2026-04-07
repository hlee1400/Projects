#include "TerrainModel.h"
#include "Model.h"
#include "d3dUtil.h"
#include "DirectXTex.h"
#include <assert.h>


TerrainModel::TerrainModel(ID3D11Device* dev, LPCWSTR heightmapFile, float len, float maxheight, float ytrans, int RepeatU, int RepeatV)
{
	DirectX::ScratchImage scrtTex;
	HRESULT hr = LoadFromTGAFile(heightmapFile, nullptr, scrtTex);
	assert(SUCCEEDED(hr));

	const DirectX::Image* hgtmap = scrtTex.GetImage(0, 0, 0);
	assert(hgtmap != nullptr);
	assert(hgtmap->height == hgtmap->width);

	size_t side = hgtmap->height;
	size_t pixel_width = 4;

	int nverts = static_cast<int>(side * side);
	StandardVertex* pVerts = new StandardVertex[nverts];

	int ntri = static_cast<int>(((side - 1) * (side - 1)) * 2);
	TriangleByIndex* pTriList = new TriangleByIndex[ntri];

	float init = len / 2.0f;
	float dist = len / static_cast<float>(side - 1);

	float uDistance = RepeatU / static_cast<float>(side - 1);
	float vDistance = RepeatV / static_cast<float>(side - 1);

	int vertIndex = 0;
	std::vector<Vect> tempNormals(nverts, Vect(0, 0, 0));

	for (int row = 0; row < static_cast<int>(side); row++) {
		float zCurrPos = init - row * dist;
		float vCurr = row * vDistance;

		for (int col = 0; col < static_cast<int>(side); col++) {
			float xCurrPos = init - col * dist;
			float uCurr = col * uDistance;
			float pixelHeight = getPixelHeight(static_cast<int>(side), row, col, pixel_width, maxheight, hgtmap);
			pVerts[vertIndex].set(xCurrPos, pixelHeight + ytrans, zCurrPos, uCurr, vCurr);
			vertIndex++;
		}
	}

	int triIndex = 0;
	for (int row = 0; row < static_cast<int>(side) - 1; row++) {
		for (int col = 0; col < static_cast<int>(side) - 1; col++) {
			int v0 = row * static_cast<int>(side) + col;
			int v1 = v0 + static_cast<int>(side);
			int v2 = v0 + 1;
			int v3 = v1 + 1;

			pTriList[triIndex].set(v0, v1, v2);
			pTriList[triIndex + 1].set(v2, v1, v3);

			Vect n1 = GetTriNormal(pVerts[v0].Pos, pVerts[v1].Pos, pVerts[v2].Pos);
			Vect n2 = GetTriNormal(pVerts[v2].Pos, pVerts[v1].Pos, pVerts[v3].Pos);

			tempNormals[v0] += n1;
			tempNormals[v1] += n1 + n2;
			tempNormals[v2] += n1 + n2;
			tempNormals[v3] += n2;

			triIndex += 2;
		}
	}

	for (int i = 1; i < side - 1; i++)
	{
		for (int j = 1; j < side - 1; j++)
		{

			Vect& pt_curr = pVerts[(i * side) + j].Pos;


			Vect& pt_1 = pVerts[((i - 1) * side) + j].Pos;
			Vect& pt_2 = pVerts[(i * side) + j - 1].Pos;
			Vect& pt_3 = pVerts[((i + 1) * side) + j - 1].Pos;
			Vect& pt_4 = pVerts[((i + 1) * side) + j].Pos;
			Vect& pt_5 = pVerts[(i * side) + j + 1].Pos;
			Vect& pt_6 = pVerts[((i - 1) * side) + j + 1].Pos;


			Vect edge1 = pt_1 - pt_curr;
			Vect edge2 = pt_2 - pt_curr;
			Vect norm1 = (edge1.cross(edge2));
			edge1 = pt_3 - pt_curr;
			Vect norm2 = (edge2.cross(edge1));
			edge2 = pt_4 - pt_curr;
			Vect norm3 = (edge1.cross(edge2));
			edge1 = pt_5 - pt_curr;
			Vect norm4 = (edge2.cross(edge1));
			edge2 = pt_6 - pt_curr;
			Vect norm5 = (edge1.cross(edge2));
			edge1 = pt_1 - pt_curr;
			Vect norm6 = (edge2.cross(edge1));

			Vect Normal = norm1 + norm2 + norm3 + norm4 + norm5 + norm6;


			pVerts[(i * side) + j].normal = (Normal * (1.0f / 6.0f));
			pVerts[(i * side) + j].normal.norm();

		}
	}

	pModTerrain = new Model(dev, pVerts, nverts, pTriList, ntri);

	delete[] pVerts;
	delete[] pTriList;
}

int TerrainModel::TexelIndex(int _side, int _row, int _col, size_t _pixel_width)
{
	int pixelIndex = _row * _side + _col;
	return pixelIndex * static_cast<int>(_pixel_width);
}

float TerrainModel::getPixelHeight(int side, int row, int col, size_t pixel_width, float maxheight, const DirectX::Image* hgtmap)
{
	int index = TexelIndex(side, row, col, pixel_width);
	unsigned char val = hgtmap->pixels[index];
	float height = (maxheight * val) / 255.0f;
	return height;
}

Vect TerrainModel::GetTriNormal(const Vect& A, const Vect& B, const Vect& C)
{
	Vect U = B - A;
	Vect V = C - A;
	Vect N = U.cross(V);
	N.norm();
	return N;
}

TerrainModel::~TerrainModel()
{
	delete pModTerrain;
}

//void TerrainModel::SetToContext(ID3D11DeviceContext* context)
//{
//	UINT stride = sizeof(StandardVertex);
//	UINT offset = 0;
//	context->IASetVertexBuffers(0, 1, &mpVertexBuffer, &stride, &offset);
//	context->IASetIndexBuffer(mpIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
//	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//}

void TerrainModel::Render(ID3D11DeviceContext* context, Texture* texture)
{
	texture->SetToContext(context);
	pModTerrain->SetToContext(context);
	pModTerrain->Render(context);
}
/*
#include "TerrainModel.h"
#include "d3dUtil.h"
#include "DirectXTex.h"
#include "ShaderTextureLight.h"
#include <assert.h>
#include <assert.h>
#include "File.h"
#include "Model.h"
#include "ModelTools.h"
#include "d3dUtil.h"
#include "AzulFileHdr.h"
#include "MeshSeparator.h"

TerrainModel::TerrainModel(ID3D11Device* dev, LPCWSTR heightmapFile, Texture* tex, ShaderTextureLight* stex, float len, float maxheight, float ytrans, int RepeatU, int RepeatV)
{
	pTex = tex;
	pShaderTexLight = stex;

	DirectX::ScratchImage scrtTex;
	HRESULT hr = LoadFromTGAFile(heightmapFile, nullptr, scrtTex);
	assert(SUCCEEDED(hr));

	const DirectX::Image* hgtmap = scrtTex.GetImage(0, 0, 0);
	assert(hgtmap->height == hgtmap->width);

	size_t side = hgtmap->height;
	size_t pixel_width = 4;

	numVerts = static_cast<int>(side * side);
	StandardVertex* pVerts = new StandardVertex[numVerts];

	numTris = static_cast<int>(((side - 1) * (side - 1)) * 2);
	TriangleByIndex* pTriList = new TriangleByIndex[numTris];

	float StartPos = len / 2;
	float distBetweenVerts = (len / (side - 1));
	float uVertDist = RepeatU / static_cast<float>(side - 1);
	float vVertDist = RepeatV / static_cast<float>(side - 1);

	for (int i = 0; i < (int)side; ++i)
	{
		float xCurrPos = StartPos;
		float currU = 0.0f;
		float currV = i * vVertDist;

		for (int j = 0; j < (int)side; ++j)
		{
			float pixelHeight = PixelHeight((int)side, j, i, pixel_width, maxheight, hgtmap);
			int index = i * (int)side + j;
			pVerts[index].set(xCurrPos, pixelHeight + ytrans, StartPos - i * distBetweenVerts, currU, currV);
			xCurrPos -= distBetweenVerts;
			currU += uVertDist;
		}
	}

	int indexCount = 0;
	for (int i = 0; i < (int)side - 1; ++i)
	{
		for (int j = 0; j < (int)side - 1; ++j)
		{
			int index = i * (int)side + j;
			pTriList[indexCount++].set(index, index + (int)side, index + 1);
			pTriList[indexCount++].set(index + 1, index + (int)side, index + 1 + (int)side);
		}
	}

	for (int i = 1; i < (int)side - 1; i++)
	{
		for (int j = 1; j < (int)side - 1; j++)
		{
			Vect& pt_curr = pVerts[(i * side) + j].Pos;
			Vect& pt_1 = pVerts[((i - 1) * side) + j].Pos;
			Vect& pt_2 = pVerts[(i * side) + j - 1].Pos;
			Vect& pt_3 = pVerts[((i + 1) * side) + j - 1].Pos;
			Vect& pt_4 = pVerts[((i + 1) * side) + j].Pos;
			Vect& pt_5 = pVerts[(i * side) + j + 1].Pos;
			Vect& pt_6 = pVerts[((i - 1) * side) + j + 1].Pos;

			Vect edge1 = pt_1 - pt_curr;
			Vect edge2 = pt_2 - pt_curr;
			Vect norm1 = (edge1.cross(edge2));
			edge1 = pt_3 - pt_curr;
			Vect norm2 = (edge2.cross(edge1));
			edge2 = pt_4 - pt_curr;
			Vect norm3 = (edge1.cross(edge2));
			edge1 = pt_5 - pt_curr;
			Vect norm4 = (edge2.cross(edge1));
			edge2 = pt_6 - pt_curr;
			Vect norm5 = (edge1.cross(edge2));
			edge1 = pt_1 - pt_curr;
			Vect norm6 = (edge2.cross(edge1));

			Vect Normal = norm1 + norm2 + norm3 + norm4 + norm5 + norm6;
			pVerts[(i * side) + j].normal = (Normal * (1.0f / 6.0f));
			pVerts[(i * side) + j].normal.norm();
		}
	}

	// Create buffers
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(StandardVertex) * numVerts;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = pVerts;

	hr = dev->CreateBuffer(&bd, &InitData, &mpVertexBuffer);
	assert(SUCCEEDED(hr));

	bd.ByteWidth = sizeof(TriangleByIndex) * numTris;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	InitData.pSysMem = pTriList;

	hr = dev->CreateBuffer(&bd, &InitData, &mpIndexBuffer);
	assert(SUCCEEDED(hr));

	delete[] pVerts;
	delete[] pTriList;
}

void TerrainModel::Render()
{
	ID3D11DeviceContext* ctx = pShaderTexLight->GetContext();
	if (!ctx) return;

	UINT stride = sizeof(StandardVertex);
	UINT offset = 0;

	ctx->IASetVertexBuffers(0, 1, &mpVertexBuffer, &stride, &offset);
	ctx->IASetIndexBuffer(mpIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pShaderTexLight->SetTextureResourceAndSampler(pTex);
	ctx->DrawIndexed(numTris * 3, 0, 0);
}

float TerrainModel::PixelHeight(int side, int k, int i, size_t pixel_width, float maxheight, const DirectX::Image* hgtmap)
{
	float h_val = hgtmap->pixels[TexelIndex(side, k, i, pixel_width)];
	return (maxheight * h_val) / 255.0f;
}

int TerrainModel::TexelIndex(int side, int i, int j, size_t pixel_width)
{
	return pixel_width * (j * side + i);
}

TerrainModel::~TerrainModel()
{
	ReleaseAndDeleteCOMobject(mpVertexBuffer);
	ReleaseAndDeleteCOMobject(mpIndexBuffer);
}

*/