// ShaderColorLight
// Andre Berthiaume, Feb 2017

#ifndef _ShaderColorLight
#define _ShaderColorLight

#include "ShaderBase.h"
#include "Matrix.h"

struct ID3D11Buffer;
struct ID3D11Device;

class ShaderColorLight : public ShaderBase
{

public:
	ShaderColorLight(const ShaderColorLight&) = delete;				 // Copy constructor
	ShaderColorLight(ShaderColorLight&&) = default;                    // Move constructor
	ShaderColorLight& operator=(const ShaderColorLight&) & = default;  // Copy assignment operator
	ShaderColorLight& operator=(ShaderColorLight&&) & = default;       // Move assignment operator
	~ShaderColorLight();		  							         // Destructor

	ShaderColorLight(ID3D11Device* device);

	virtual void SetToContext(ID3D11DeviceContext* devcon);

	void SetDirectionalLightParameters(const Vect& dir, const Vect& amb = Vect(1, 1, 1), const Vect& dif = Vect(1, 1, 1), const Vect& sp = Vect(1, 1, 1));
	void SetPointLightParameters(const Vect& pos, float r, const Vect& att, const Vect& amb = Vect(1, 1, 1), const Vect& dif = Vect(1, 1, 1), const Vect& sp = Vect(1, 1, 1), int nLights = 3);
	void SetSpotLightParameters(const Vect& pos, float r, const Vect& att, const Vect& dir, float spotExp, const Vect& amb = Vect(1, 1, 1), const Vect& dif = Vect(1, 1, 1), const Vect& sp = Vect(1, 1, 1), int nLights = 3);

	void SendCamMatrices(const Matrix& view, const Matrix& proj);
	void SendLightParameters(const Vect& eyepos);
	void SendWorldAndMaterial(const Matrix& world, const Vect& amb = Vect(.5f, .5f, .5f), const Vect& dif = Vect(.5f, .5f, .5f), const Vect& sp = Vect(.5f, .5f, .5f));

	void SetNSendFogParameters(const float& fogStart = 0, const float& fogRange = 0, const Vect& fogColor = Vect(.2, .2, .2, 1));
	//void SendFogParameters();

private:

	static const int numLights = 3;


	struct Fog
	{
		float FogStart;
		float FogRange;
		Vect FogColor;
	};

	Fog fogData;
	ID3D11Buffer* mpBufferFog;

	struct Material
	{
		Vect Ambient;
		Vect Diffuse;
		Vect Specular;
	};

	struct PhongADS
	{
		Vect Ambient;
		Vect Diffuse;
		Vect Specular;
	};

	struct DirectionalLight
	{
		PhongADS Light;
		Vect Direction;
	};

	DirectionalLight DirLightData;

	struct PointLight
	{
		PhongADS Light;
		Vect Position;
		Vect Attenuation;
		float Range;
	};


	struct SpotLight
	{
		PhongADS Light;
		Vect Position;
		Vect Attenuation;
		Vect Direction;
		float SpotExp;
		float Range;
	};


	struct CamMatrices
	{
		Matrix View;
		Matrix Projection;
	};

	ID3D11Buffer*  mpBufferCamMatrices;

	struct Data_WorldAndMaterial
	{
		Matrix World;
		Matrix WorlInv;
		Material Mat;
	};

	ID3D11Buffer*	mpBuffWordAndMaterial;

	struct Data_LightParams
	{
		DirectionalLight DirLight;
		PointLight PntLight[numLights];
		SpotLight SptLight[numLights];
		Vect EyePosWorld;
	};

	ID3D11Buffer*  mpBufferLightParams;
	PointLight PointLightData[numLights];
	SpotLight SpotLightData[numLights];


};

#endif _ShaderColorLight

