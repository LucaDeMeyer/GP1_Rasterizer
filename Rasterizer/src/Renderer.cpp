//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"


#include "Maths.h"
#include "Texture.h"
#include "Utils.h"

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);
	

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	
	m_pDepthBufferPixels = new float[static_cast<float>(m_Width * m_Height)];


	std::vector<Vertex> vertices;
	std::vector<Uint32> indices;
	//TukTuk
	//Utils::ParseOBJ("Resources/tuktuk.obj", vertices, indices);

	//load obj
	Utils::ParseOBJ("Resources/vehicle.obj", vertices, indices);
	m_MeshesWorld.emplace_back(vertices, indices, PrimitiveTopology::TriangleList);

	//load textures
	//m_pTexture = Texture::LoadFromFile("Resources/tuktuk.png");
	m_pTexture = Texture::LoadFromFile("resources/vehicle_diffuse.png");
	m_pNormalMap = Texture::LoadFromFile("resources/vehicle_normal.png");
	m_pSpecularMap = Texture::LoadFromFile("resources/vehicle_specular.png");
	m_pGlossinessMap = Texture::LoadFromFile("resources/vehicle_gloss.png");


	//Initialize Camera
	m_Ar = static_cast<float>(m_Width) / static_cast<float>(m_Height);
	m_Camera.Initialize( m_Ar,45.f, { 0.f, 5.f, -50.f });

	const Vector3 position{ 0, 0, 50 };

	constexpr float YRotation{ -PI_DIV_2 };

	m_MeshesWorld[0].worldMatrix = Matrix::CreateRotationY(YRotation) * Matrix::CreateTranslation(position);
	m_MeshOriginalWorldMatrix = m_MeshesWorld[0].worldMatrix;

}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	delete m_pTexture;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
	HandleKeyInput();
	if (m_State == State::rotate)
	{
		RotateMesh(pTimer->GetElapsed());
	}
	Matrix rotationMatrix{ Matrix::CreateRotationY(m_MeshRotationAngle) };
	m_MeshesWorld[0].worldMatrix = rotationMatrix * m_MeshOriginalWorldMatrix;
}


void Renderer::Render() const
{
	//clear BackGround and reset DepthBuffer
	ClearBackground();
	ResetDepthBuffer();

	// for each mesh
	for (const auto& mesh : m_MeshesWorld)
	{
		const auto worldViewProjectionMatrix = mesh.worldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix;

		std::vector<Vertex_Out> vertices_ndc{};
		std::vector<Vector2>vertices_screen{};

		VertexTransformationFunction(mesh.vertices, vertices_ndc, worldViewProjectionMatrix, mesh.worldMatrix);

		VertexTransformationToScreenSpace(vertices_ndc, vertices_screen);

		//if triangle list -> go over indices by 3 to get full triangle
		if (mesh.primitiveTopology == PrimitiveTopology::TriangleList)
		{
			for (size_t vertexIndex{}; vertexIndex < mesh.indices.size(); vertexIndex += 3)
			{
				//get vertex index
				uint32_t vertexIndex0 = { mesh.indices[vertexIndex] };
				uint32_t vertexIndex1 = { mesh.indices[vertexIndex + 1] };
				uint32_t vertexIndex2 = { mesh.indices[vertexIndex + 2] };

				//get vertices
				const Vector2 v0{ vertices_screen[vertexIndex0] };
				const Vector2 v1{ vertices_screen[vertexIndex1] };
				const Vector2 v2{ vertices_screen[vertexIndex2] };

				//frustrum culling
				if (Camera::IsOutsideFrustum(vertices_ndc[vertexIndex0].position)) continue;
				if (Camera::IsOutsideFrustum(vertices_ndc[vertexIndex1].position)) continue;
				if (Camera::IsOutsideFrustum(vertices_ndc[vertexIndex2].position)) continue;


				//calc edges
				const Vector2 edge01 = v1 - v0;
				const Vector2 edge12 = v2 - v1;
				const Vector2 edge20 = v0 - v2;

				//calc triangle area
				const float fullTriangleArea{ Vector2::Cross(v1 - v0, v2 - v0) };

				const int boundingBoxpadding{ 1 };
				// Calculate bounding box  -> add/subtract 1 -> gets rid of lines between triangles
				int minX = static_cast<int>(std::min({ v0.x, v1.x, v2.x })) - boundingBoxpadding;
				int minY = static_cast<int>(std::min({ v0.y, v1.y, v2.y })) - boundingBoxpadding;
				int maxX = static_cast<int>(std::max({ v0.x, v1.x, v2.x })) + boundingBoxpadding;
				int maxY = static_cast<int>(std::max({ v0.y, v1.y, v2.y })) + boundingBoxpadding;

				// Clamp bounding box within screen bounds
				minX = std::max(minX, 0);
				minY = std::max(minY, 0);
				maxX = std::min(maxX, m_Width - 1);
				maxY = std::min(maxY, m_Height - 1);

				//for each pixel
				for (int px{ minX }; px < maxX; ++px)
				{
					for (int py{ minY }; py < maxY; ++py)
					{

						ColorRGB finalColor{ 0,0,0 };
						const int pixelIdx{ px + py * m_Width };
						const Vector2 pixel{ static_cast<float>(px),static_cast<float>(py) };

						// Calc the vector between vertex and pixel
						const Vector2 directionV0{ pixel - v0 };
						const Vector2 directionV1{ pixel - v1 };
						const Vector2 directionV2{ pixel - v2 };

						// Calc the barycentric weights
						float weightV0{ Vector2::Cross(edge12 , directionV1) };
						float weightV1{ Vector2::Cross(edge20,directionV2) };
						float weightV2{ Vector2::Cross(edge01,directionV0) };

						//hit-test
						if (weightV0 < 0)
							continue;
						if (weightV1 < 0)
							continue;
						if (weightV2 < 0)
							continue;

						weightV0 /= fullTriangleArea;
						weightV1 /= fullTriangleArea;
						weightV2 /= fullTriangleArea;



						//Calculate the depth
						const float depthV0{ (vertices_ndc[vertexIndex0].position.z) };
						const float depthV1{ (vertices_ndc[vertexIndex1].position.z) };
						const float depthV2{ (vertices_ndc[vertexIndex2].position.z) };

						// Calculate the depth at this pixel
						const float interpolatedDepth
						{
							1.0f /
							(weightV0 * 1.0f / depthV0 +
								weightV1 * 1.0f / depthV1 +
								weightV2 * 1.0f / depthV2)
						};

						
						if (m_pDepthBufferPixels[pixelIdx] < interpolatedDepth) continue;

						// Save the new depth
						m_pDepthBufferPixels[pixelIdx] = interpolatedDepth;

						
						//calculate WDepth
						const float wDepthV0{ (vertices_ndc[vertexIndex0].position.w) };
						const float wDepthV1{ (vertices_ndc[vertexIndex1].position.w) };
						const float wDepthV2{ (vertices_ndc[vertexIndex2].position.w) };


						//Update Color in Buffer
						switch (m_displayMode)
						{
						case DisplayMode::finalColor:

						{
							const float interpolatedWDepth
							{

								1.f /
								(weightV0 / wDepthV0 + weightV1 / wDepthV1 + weightV2 / wDepthV2)
							};

							Vector2 interpolatedUv
							{
								((vertices_ndc[vertexIndex0].uv / wDepthV0) * weightV0 +
								(vertices_ndc[vertexIndex1].uv / wDepthV1) * weightV1 +
								(vertices_ndc[vertexIndex2].uv / wDepthV2) * weightV2) * interpolatedWDepth

							};
							Vector3 interpolatedNormal
							{
									((vertices_ndc[vertexIndex0].normal / wDepthV0) * weightV0 +
								(vertices_ndc[vertexIndex1].normal / wDepthV1) * weightV1 +
								(vertices_ndc[vertexIndex2].normal / wDepthV2) * weightV2)* interpolatedWDepth
							};

							interpolatedNormal.Normalize();

							Vector3 interpolatedTangent
							{
								((vertices_ndc[vertexIndex0].tangent / wDepthV0) * weightV0 +
								(vertices_ndc[vertexIndex1].tangent / wDepthV1) * weightV1 +
								(vertices_ndc[vertexIndex2].tangent / wDepthV2) * weightV2) * interpolatedWDepth

							};
							interpolatedTangent.Normalize();

							Vector3 interpolatedViewDirection
							{
								((vertices_ndc[vertexIndex0].viewDirection / wDepthV0) * weightV0 +
								(vertices_ndc[vertexIndex1].viewDirection / wDepthV1) * weightV1 +
								(vertices_ndc[vertexIndex2].viewDirection / wDepthV2) * weightV2)* interpolatedWDepth
							};
							Vertex_Out pixelVertex{};
							pixelVertex.position = Vector4{ pixel.x,pixel.y,interpolatedDepth,interpolatedWDepth };
							pixelVertex.color = ColorRGB{ 0,0,0 };
							pixelVertex.uv = interpolatedUv;
							pixelVertex.normal = interpolatedNormal;
							pixelVertex.tangent = interpolatedTangent;
							pixelVertex.viewDirection = interpolatedViewDirection;

							finalColor = PixelShading(pixelVertex);
							break;
						}

						case DisplayMode::depthBuffer:
						{
							const float depthBufferColor = Remap(m_pDepthBufferPixels[px + (py * m_Width)], 0.995f, 1.0f);

							finalColor = { depthBufferColor, depthBufferColor, depthBufferColor };
							break;
						}
						}

						finalColor.MaxToOne();

						m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
							static_cast<uint8_t>(finalColor.r * 255),
							static_cast<uint8_t>(finalColor.g * 255),
							static_cast<uint8_t>(finalColor.b * 255));
					}
				}
			}
		}
		//@END
		//Update SDL Surface
		SDL_UnlockSurface(m_pBackBuffer);
		SDL_BlitSurface(m_pBackBuffer, nullptr, m_pFrontBuffer, nullptr);
		SDL_UpdateWindowSurface(m_pWindow);
	}
}

void Renderer::RenderTriangleStrip(std::vector<Mesh>& meshes_world, std::vector<Vertex_Out>& vertices_ndc,std::vector<Vector2>&vertices_screen) const
{
	for (const auto& mesh : meshes_world)
	{
		//if triangle strip -> loop over indices by 1
		if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip)
		{
			for (int vertexIndex{}; vertexIndex < mesh.indices.size() - 2; ++vertexIndex)
			{
				// bool top swap vertices -> uneven index -> flip triangle
				bool swapVertices = vertexIndex % 2;

				uint32_t vertexIndex0 = { mesh.indices[vertexIndex] };
				uint32_t vertexIndex1 = { mesh.indices[vertexIndex + 1 * !swapVertices + 2 * swapVertices] };
				uint32_t vertexIndex2 = { mesh.indices[vertexIndex + 2 * !swapVertices + 1 * swapVertices] };

				//frustrum clipping
				if (Camera::IsOutsideFrustum(vertices_ndc[vertexIndex0].position)) continue;
				if (Camera::IsOutsideFrustum(vertices_ndc[vertexIndex1].position)) continue;
				if (Camera::IsOutsideFrustum(vertices_ndc[vertexIndex2].position)) continue;

				//get vertices
				const Vector2 v0{ vertices_screen[vertexIndex0] };
				const Vector2 v1{ vertices_screen[vertexIndex1] };
				const Vector2 v2{ vertices_screen[vertexIndex2] };

				//calc edges

				const Vector2 edge01 = v1 - v0;
				const Vector2 edge12 = v2 - v1;
				const Vector2 edge20 = v0 - v2;

				//calc triangle area
				const float fullTriangleArea{ Vector2::Cross(v1 - v0, v2 - v0) };

				const int boundingBoxpadding{ 1 };
				// Calculate bounding box of the triangle -> add/subtract 1 -> gets rid of lines between triangles
				int minX = static_cast<int>(std::min({ v0.x, v1.x, v2.x })) - boundingBoxpadding;
				int minY = static_cast<int>(std::min({ v0.y, v1.y, v2.y })) - boundingBoxpadding;
				int maxX = static_cast<int>(std::max({ v0.x, v1.x, v2.x })) + boundingBoxpadding;
				int maxY = static_cast<int>(std::max({ v0.y, v1.y, v2.y })) + boundingBoxpadding;

				// Clamp bounding box within screen bounds
				minX = std::max(minX, 0);
				minY = std::max(minY, 0);
				maxX = std::min(maxX, m_Width - 1);
				maxY = std::min(maxY, m_Height - 1);

				for (int px{ minX }; px < maxX; ++px)
				{
					for (int py{ minY }; py < maxY; ++py)
					{

						ColorRGB finalColor{ 0,0,0 };
						const int pixelIdx{ px + py * m_Width };
						const Vector2 pixel{ static_cast<float>(px),static_cast<float>(py) };

						// Calculate the vector between vertex and the point
						const Vector2 directionV0{ pixel - v0 };
						const Vector2 directionV1{ pixel - v1 };
						const Vector2 directionV2{ pixel - v2 };

						// Calculate the barycentric weights
						float weightV0{ Vector2::Cross(edge12 , directionV1) };
						float weightV1{ Vector2::Cross(edge20,directionV2) };
						float weightV2{ Vector2::Cross(edge01,directionV0) };

						//hit-test
						if (weightV0 < 0)
							continue;
						if (weightV1 < 0)
							continue;
						if (weightV2 < 0)
							continue;

						weightV0 /= fullTriangleArea;
						weightV1 /= fullTriangleArea;
						weightV2 /= fullTriangleArea;



						//Calculate the depth
						const float depthV0{ (vertices_ndc[vertexIndex0].position.z) };
						const float depthV1{ (vertices_ndc[vertexIndex1].position.z) };
						const float depthV2{ (vertices_ndc[vertexIndex2].position.z) };

						// Calculate the depth at this pixel
						const float interpolatedDepth
						{
							1.0f /
							(weightV0 * 1.0f / depthV0 +
								weightV1 * 1.0f / depthV1 +
								weightV2 * 1.0f / depthV2)
						};

						// If this pixel hit is further away then a previous pixel hit, continue to the next pixel
						if (m_pDepthBufferPixels[pixelIdx] < interpolatedDepth) continue;

						// Save the new depth
						m_pDepthBufferPixels[pixelIdx] = interpolatedDepth;

						//calculate WDepth
						const float wDepthV0{ (vertices_ndc[vertexIndex0].position.w) };
						const float wDepthV1{ (vertices_ndc[vertexIndex1].position.w) };
						const float wDepthV2{ (vertices_ndc[vertexIndex2].position.w) };


						//Update Color in Buffer
						switch (m_displayMode)
						{
						case DisplayMode::finalColor:

						{
							const float interpolatedWDepth
							{

								1.f /
								(weightV0 / wDepthV0 + weightV1 / wDepthV1 + weightV2 / wDepthV2)
							};

							Vector2 interpolatedUv
							{
								((vertices_ndc[vertexIndex0].uv / wDepthV0) * weightV0 +
								(vertices_ndc[vertexIndex1].uv / wDepthV1) * weightV1 +
								(vertices_ndc[vertexIndex2].uv / wDepthV2) * weightV2) * interpolatedWDepth

							};
							//clamp uv x and y to [0,1]
							interpolatedUv.x = std::clamp(interpolatedUv.x, 0.0f, 1.f);
							interpolatedUv.y = std::clamp(interpolatedUv.y, 0.f, 1.f);

							finalColor = m_pTexture->Sample(interpolatedUv);
							break;
						}
						case DisplayMode::depthBuffer:
						{
							constexpr float minValue = 0.8f;
							constexpr float maxValue = 1.f;

							// Remap the value to the range [0, 1]
							float remappedValue = (interpolatedDepth - minValue) / (maxValue - minValue);
							remappedValue = std::clamp(remappedValue, 0.f, 1.f);

							finalColor = ColorRGB{ remappedValue, remappedValue, remappedValue };
							break;
						}
						}

						finalColor.MaxToOne();

						m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
							static_cast<uint8_t>(finalColor.r * 255),
							static_cast<uint8_t>(finalColor.g * 255),
							static_cast<uint8_t>(finalColor.b * 255));
					}
				}
			}
		}
	}
}


void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
	//Todo > W1 Projection Stage

	//Convert to NDC > SCREEN space
	vertices_out.resize(vertices_in.size());
	for (size_t i{}; i < vertices_in.size(); ++i)
	{
		//Transform them with a VIEW Matrix (inverse ONB)
		vertices_out[i].position = m_Camera.viewMatrix.TransformPoint({ vertices_in[i].position, 1.0f });
		vertices_out[i].color = vertices_in[i].color;

		//Perspective Divide
		vertices_out[i].position.x /= vertices_out[i].position.z;
		vertices_out[i].position.y /= vertices_out[i].position.z;

	}
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex_Out>& vertices_out, const Matrix& worldViewProjectionMatrix, const Matrix& meshWorldMatrix) const
{
	vertices_out.resize(vertices_in.size());

	for (size_t i{}; i < vertices_in.size(); ++i)
	{
		// Transform with VIEW matrix (inverse ONB)
		vertices_out[i].position = worldViewProjectionMatrix.TransformPoint({ vertices_in[i].position, 1.0f });
		vertices_out[i].normal = meshWorldMatrix.TransformVector(vertices_in[i].normal);
		vertices_out[i].tangent = meshWorldMatrix.TransformVector(vertices_in[i].tangent);

		vertices_out[i].uv = vertices_in[i].uv;

		// Apply Perspective Divide
		vertices_out[i].position.x /= vertices_out[i].position.w;
		vertices_out[i].position.y /= vertices_out[i].position.w;
		vertices_out[i].position.z /= vertices_out[i].position.w;

	}
}

void Renderer::VertexTransformationToScreenSpace(const std::vector<Vertex_Out>& vertices_in,
	std::vector<Vector2>& vertex_out) const
{
	vertex_out.reserve(vertices_in.size());
	for (const Vertex_Out& ndcVertex : vertices_in)
	{
		vertex_out.emplace_back(
			m_Width * ((ndcVertex.position.x + 1) / 2.0f),
			m_Height * ((1.0f - ndcVertex.position.y) / 2.0f)
		);
	}
}

ColorRGB Renderer::PixelShading( Vertex_Out& v) const
{
	Vector3 lightDirection{ .557f,-.557f,.557f };

	lightDirection.Normalize();
	constexpr float lightIntensity{ 2.f };
	constexpr float specularShininess{ 25.f };

	if (m_UseNormalMap)
	{
		const Vector3 biNormal = Vector3::Cross(v.normal, v.tangent);
		const Matrix tangentSpaceAxis = { v.tangent, biNormal, v.normal, Vector3::Zero };

		const ColorRGB normalColor = m_pNormalMap->Sample(v.uv);
		Vector3 sampledNormal = { normalColor.r, normalColor.g, normalColor.b };
		sampledNormal = 2.f * sampledNormal - Vector3{ 1.f, 1.f, 1.f };

		sampledNormal = tangentSpaceAxis.TransformVector(sampledNormal);

		v.normal = sampledNormal.Normalized();
	}


	// OBSERVED AREA
	float ObservedArea{ Vector3::Dot(v.normal,  -lightDirection) };
	ObservedArea = std::max(ObservedArea, 0.f);

	const ColorRGB observedAreaRGB{ ObservedArea ,ObservedArea ,ObservedArea };

	// DIFFUSE
	const ColorRGB TextureColor{ m_pTexture->Sample(v.uv) };

	// SPECULAR
	const Vector3 reflect{ Vector3::Reflect(-lightDirection, v.normal) };
	float cosAlpha{ Vector3::Dot(reflect, v.viewDirection) };
	cosAlpha = std::max(0.f, cosAlpha);


	const float specularExp{ specularShininess * m_pGlossinessMap->Sample(v.uv).r };

	const ColorRGB specular{ m_pSpecularMap->Sample(v.uv) * powf(cosAlpha, specularExp) };

	ColorRGB finalColor{ 0,0,0 };

	switch (m_ShadingMode)
	{
	case ShadingMode::observed:
	{
		finalColor += observedAreaRGB;
		break;
	}
	case ShadingMode::diffuse:
	{
		finalColor += lightIntensity * observedAreaRGB * TextureColor / PI;
		break;
	}
	case ShadingMode::specular:
	{
		finalColor += specular; // *observedAreaRGB;
		break;
	}
	case ShadingMode::combined:
	{
		finalColor += (lightIntensity * TextureColor / PI + specular) * observedAreaRGB;
		break;
	}
	}

	const ColorRGB ambient{ .05f,.05f,.05f };
	finalColor += ambient;
	finalColor.MaxToOne();

	return finalColor;
}

void Renderer::RotateMesh(float elapsedSec)
{

	constexpr float rotationSpeed{ 1.f };
	m_MeshRotationAngle += rotationSpeed * elapsedSec;
}

//void Renderer::RenderTriangle() const
//{
//	//Triangle
//	std::vector<Vertex> vertices_ndc{};
//	const std::vector<Vertex> vertices_world
//	{
//		// Triangle 0
//				{{0.f,2.f,0.f},{1,0,0}},
//				{{1.5f,-1.f,0.f},{1,0,0}},
//				{{-1.5f,-1.f,0.f},{1,0,0}},
//
//				// Triangle 1
//				{{0.f,4.f,2.f},{1,0,0}},
//				{{3.f,-2.f,2.f},{0,1,0}},
//				{{-3.f,-2.f,2.f},{0,0,1}}
//	};
//
//	std::vector<Vector2> vertices_raster{};
//
//	VertexTransformationFunction(vertices_world, vertices_ndc);
//	VertexTransformationToScreenSpace(vertices_ndc, vertices_raster);
//
//
//	for (size_t i{}; i < vertices_world.size(); i += 3)
//	{
//		const Vector2 v0 = vertices_raster[i];
//		const Vector2 v1 = vertices_raster[i + 1];
//		const Vector2 v2 = vertices_raster[i + 2];
//
//		const Vector2 edge01 = v1 - v0;
//		const Vector2 edge12 = v2 - v1;
//		const Vector2 edge20 = v0 - v2;
//
//
//		ColorRGB colorV0 = vertices_world[i].color;
//		ColorRGB colorV1 = vertices_world[i + 1].color;
//		ColorRGB colorV2 = vertices_world[i + 2].color;
//
//
//	
//
//		const float fullTriangleArea{ Vector2::Cross(v1 - v0, v2 - v0) };
//
//		// Calculate bounding box of the triangle
//		int minX = static_cast<int>(std::min({ v0.x, v1.x, v2.x }));
//		int minY = static_cast<int>(std::min({ v0.y, v1.y, v2.y }));
//		int maxX = static_cast<int>(std::max({ v0.x, v1.x, v2.x }));
//		int maxY = static_cast<int>(std::max({ v0.y, v1.y, v2.y }));
//
//		// Clamp bounding box within screen bounds
//		minX = std::max(minX, 0);
//		minY = std::max(minY, 0);
//		maxX = std::min(maxX, m_Width - 1);
//		maxY = std::min(maxY, m_Height - 1);
//
//		for (int px{ minX }; px < maxX; ++px)
//		{
//			for (int py{ minY }; py < maxY; ++py)
//			{
//
//				ColorRGB finalColor{ 0,0,0 };
//
//				const Vector2 pixel{ static_cast<float>(px),static_cast<float>(py) };
//
//				// Calculate the vector between vertex and the point
//				const Vector2 directionV0{ pixel - v0 };
//				const Vector2 directionV1{ pixel - v1 };
//				const Vector2 directionV2{ pixel - v2 };
//
//				// Calculate the barycentric weights
//				float weightV0{ Vector2::Cross(edge12 , directionV1) };
//				float weightV1{ Vector2::Cross(edge20,directionV2) };
//				float weightV2{ Vector2::Cross(edge01,directionV0) };
//
//				//hit-test
//				if (weightV0 < 0)
//					continue;
//				if (weightV1 < 0)
//					continue;
//				if (weightV2 < 0)
//					continue;
//
//				weightV0 /= fullTriangleArea;
//				weightV1 /= fullTriangleArea;
//				weightV2 /= fullTriangleArea;
//
//
//				const float depthWeight =
//				{
//					weightV0 * vertices_world[i].position.z +
//					weightV1 * vertices_world[i + 1].position.z +
//					weightV2 * vertices_world[i + 2].position.z
//				};
//
//				if (depthWeight > m_pDepthBufferPixels[px * m_Height + py])
//					continue;
//
//				m_pDepthBufferPixels[px * m_Height + py] = depthWeight;
//
//
//
//				finalColor = colorV0 * weightV0 + colorV1 * weightV1 + colorV2 * weightV2;
//				//Update Color in Buffer
//				finalColor.MaxToOne();
//
//				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
//					static_cast<uint8_t>(finalColor.r * 255),
//					static_cast<uint8_t>(finalColor.g * 255),
//					static_cast<uint8_t>(finalColor.b * 255));
//			}
//		}
//	}
//
//
//}


void Renderer::HandleKeyInput()
{
	const uint8_t* pKeyboardState = SDL_GetKeyboardState(nullptr);
	if (pKeyboardState[SDL_SCANCODE_Z])
	{
		switch (m_displayMode)
		{
		case DisplayMode::finalColor:
		{
			m_displayMode = DisplayMode::depthBuffer;
			break;
		}
		case DisplayMode::depthBuffer:
		{
			m_displayMode = DisplayMode::finalColor;
			break;
		}
		}
	}
	if(pKeyboardState[SDL_SCANCODE_N])
	{
		switch(m_UseNormalMap)
		{
		case true:
		{
			m_UseNormalMap = false;
			break;
		}
		case false:
		{
			m_UseNormalMap = true;
			break;
		}

		}
	}

	if(pKeyboardState[SDL_SCANCODE_R])
	{
		switch(m_State)
		{
		case State::idle:
		{
			m_State = State::rotate;
			break;
		}
		case State::rotate:
		{
			m_State = State::idle;
			break;
		}
		}
	}

	if(pKeyboardState[SDL_SCANCODE_F7])
	{
		switch(m_ShadingMode)
		{
		case ShadingMode::combined:
		{
			m_ShadingMode = ShadingMode::diffuse;
			break;
		}
		case ShadingMode::diffuse:
		{
			m_ShadingMode = ShadingMode::observed;
			break;
		}
		case ShadingMode::observed:
		{
			m_ShadingMode = ShadingMode::specular;
			break;
		}
		case ShadingMode::specular:
		{
			m_ShadingMode = ShadingMode::combined;
			break;
		}
		}
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}


void Renderer::ClearBackground() const
{
	SDL_FillRect(m_pBackBuffer, nullptr, SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100));
}

void Renderer::ResetDepthBuffer() const
{
	const int nrPixels{ m_Width * m_Height };
	std::fill_n(m_pDepthBufferPixels, nrPixels, FLT_MAX);
}



