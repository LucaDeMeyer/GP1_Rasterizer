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

	m_pTexture = Texture::LoadFromFile("Resources/uv_grid_2.png");

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });

	m_Ar = static_cast<float>(m_Width) / static_cast<float>(m_Height);
	
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	delete m_pTexture;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}
//Luca
void Renderer::Render() const
{

	ClearBackground();
	ResetDepthBuffer();

	//define mesh
	const std::vector<Mesh> meshes_world =
	{

		Mesh{
			{
				Vertex{{-3.f, 3.f, -2.f}, {0.0f, 0.0f},{0,0}},
				Vertex{{0.f, 3.f, -2.f}, {0.5f, 0.0f},{.5,0}},
				Vertex{{3.f, 3.f, -2.f}, {1.0f, 0.0f},{1,0}},
				Vertex{{-3.f, 0.f, -2.f}, {0.0f, 0.5f},{0,.5}},
				Vertex{{0.f, 0.f, -2.f}, {0.5f, 0.5f},{.5,.5}},
				Vertex{{3.f, 0.f, -2.f}, {1.0f, 0.5f},{1,.5}},
				Vertex{{-3.f, -3.f, -2.f}, {0.0f, 1.0f},{0,1}},
				Vertex{{0.f, -3.f, -2.f}, {0.5f, 1.0f},{.5,1}},
				Vertex{{3.f, -3.f, -2.f}, {1.0f, 1.0f},{1,1}},
			},

			{
				3, 0, 4, 1, 5, 2,
				2, 6,
				6, 3, 7, 4, 8, 5
			},

			PrimitiveTopology::TriangleStrip
		}
	};

	// for each mesh

	for(const auto& mesh : meshes_world)
	{
		std::vector<Vertex> vertices_ndc{};
		std::vector<Vector2>vertices_screen{};

		//transform from world to NDC
		VertexTransformationFunction(mesh.vertices, vertices_ndc);
		VertexTransformationToScreenSpace(vertices_ndc, vertices_screen);

		//if triangle list -> go over indices by 3 to get full triangle
		if(mesh.primitiveTopology == PrimitiveTopology::TriangleList)
		{
			for(size_t vertexIndex{}; vertexIndex < mesh.indices.size(); vertexIndex+=3)
			{
				uint32_t vertexIndex0 = { mesh.indices[vertexIndex] };
				uint32_t vertexIndex1 = { mesh.indices[vertexIndex + 1] };
				uint32_t vertexIndex2 = { mesh.indices[vertexIndex + 2] };

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

				// Calculate bounding box of the triangle
				int minX = static_cast<int>(std::min({ v0.x, v1.x, v2.x }));
				int minY = static_cast<int>(std::min({ v0.y, v1.y, v2.y }));
				int maxX = static_cast<int>(std::max({ v0.x, v1.x, v2.x }));
				int maxY = static_cast<int>(std::max({ v0.y, v1.y, v2.y }));

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

						//Update Color in Buffer
						finalColor.MaxToOne();

						m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
							static_cast<uint8_t>(finalColor.r * 255),
							static_cast<uint8_t>(finalColor.g * 255),
							static_cast<uint8_t>(finalColor.b * 255));
					}
				}
			}


		}


		//if triangle strip -> loop over indices by 1
		if(mesh.primitiveTopology == PrimitiveTopology::TriangleStrip)
		{
			for(int vertexIndex{}; vertexIndex < mesh.indices.size()- 2; ++vertexIndex)
			{
				// bool top swap vertices -> uneven index -> flip triangle
				bool swapVertices = vertexIndex % 2;

				
				uint32_t vertexIndex0 = { mesh.indices[vertexIndex] };
				uint32_t vertexIndex1 = {mesh.indices[vertexIndex + 1 * !swapVertices + 2 * swapVertices]};
				uint32_t vertexIndex2 = {mesh.indices[vertexIndex + 2 * !swapVertices + 1 * swapVertices]};

				// same logic as render triangle

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

				// Calculate bounding box of the triangle
				int minX = static_cast<int>(std::min({ v0.x, v1.x, v2.x }));
				int minY = static_cast<int>(std::min({ v0.y, v1.y, v2.y }));
				int maxX = static_cast<int>(std::max({ v0.x, v1.x, v2.x }));
				int maxY = static_cast<int>(std::max({ v0.y, v1.y, v2.y }));

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

						const Vector2 interpolatedUv
						{
							((vertices_ndc[vertexIndex0].uv / depthV0) * weightV0 +
							(vertices_ndc[vertexIndex1].uv / depthV1) * weightV1 + 
							(vertices_ndc[vertexIndex2].uv / depthV2) * weightV2) * interpolatedDepth

						};


						//Update Color in Buffer
						finalColor = m_pTexture->Sample(interpolatedUv);
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
	
	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, nullptr, m_pFrontBuffer, nullptr);
	SDL_UpdateWindowSurface(m_pWindow);
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


void Renderer::VertexTransformationToScreenSpace(const std::vector<Vertex>& vertices_in,
	std::vector<Vector2>& vertex_out) const
{
	vertex_out.reserve(vertices_in.size());
	for (const Vertex& ndcVertex : vertices_in)
	{
		vertex_out.emplace_back(
			m_Width * ((ndcVertex.position.x + 1) / 2.0f),
			m_Height * ((1.0f - ndcVertex.position.y) / 2.0f)
		);
	}
}



void Renderer::RenderTriangle() const
{
	//Triangle
	std::vector<Vertex> vertices_ndc{};
	const std::vector<Vertex> vertices_world
	{
		// Triangle 0
				{{0.f,2.f,0.f},{1,0,0}},
				{{1.5f,-1.f,0.f},{1,0,0}},
				{{-1.5f,-1.f,0.f},{1,0,0}},

				// Triangle 1
				{{0.f,4.f,2.f},{1,0,0}},
				{{3.f,-2.f,2.f},{0,1,0}},
				{{-3.f,-2.f,2.f},{0,0,1}}
	};

	std::vector<Vector2> vertices_raster{};

	VertexTransformationFunction(vertices_world, vertices_ndc);
	VertexTransformationToScreenSpace(vertices_ndc, vertices_raster);


	for (size_t i{}; i < vertices_world.size(); i += 3)
	{
		const Vector2 v0 = vertices_raster[i];
		const Vector2 v1 = vertices_raster[i + 1];
		const Vector2 v2 = vertices_raster[i + 2];

		const Vector2 edge01 = v1 - v0;
		const Vector2 edge12 = v2 - v1;
		const Vector2 edge20 = v0 - v2;


		ColorRGB colorV0 = vertices_world[i].color;
		ColorRGB colorV1 = vertices_world[i + 1].color;
		ColorRGB colorV2 = vertices_world[i + 2].color;


	

		const float fullTriangleArea{ Vector2::Cross(v1 - v0, v2 - v0) };

		// Calculate bounding box of the triangle
		int minX = static_cast<int>(std::min({ v0.x, v1.x, v2.x }));
		int minY = static_cast<int>(std::min({ v0.y, v1.y, v2.y }));
		int maxX = static_cast<int>(std::max({ v0.x, v1.x, v2.x }));
		int maxY = static_cast<int>(std::max({ v0.y, v1.y, v2.y }));

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


				const float depthWeight =
				{
					weightV0 * vertices_world[i].position.z +
					weightV1 * vertices_world[i + 1].position.z +
					weightV2 * vertices_world[i + 2].position.z
				};

				if (depthWeight > m_pDepthBufferPixels[px * m_Height + py])
					continue;

				m_pDepthBufferPixels[px * m_Height + py] = depthWeight;



				finalColor = colorV0 * weightV0 + colorV1 * weightV1 + colorV2 * weightV2;
				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
			}
		}
	}


}



void Renderer::RenderTriangle(const Mesh& mesh, const std::vector<Vector2>& verteciesRaster,
	int vertexIndex, bool swapVertex) const
{
	const size_t vertexIndex0{ mesh.indices[vertexIndex + (2 * swapVertex)] };
	const size_t vertexIndex1{ mesh.indices[vertexIndex + 1] };
	const size_t vertexIndex2{ mesh.indices[vertexIndex + (!swapVertex * 2)] };

	
	// Make sure the triangle doesn't have the same vertex twice. If it does it's got no area so we don't have to render it.
	if (vertexIndex0 == vertexIndex1 || vertexIndex1 == vertexIndex2 || vertexIndex2 == vertexIndex0)
		return;

	const Vector2 v0{ verteciesRaster[vertexIndex0] };
	const Vector2 v1{ verteciesRaster[vertexIndex1] };
	const Vector2 v2{ verteciesRaster[vertexIndex2] };

	//const Vector2 v0 = { mesh.vertices[vertexIndex0].position.x, mesh.vertices[vertexIndex0].position.y };
	//const Vector2 v1 = { mesh.vertices[vertexIndex1].position.x, mesh.vertices[vertexIndex1].position.y };
	//const Vector2 v2 = { mesh.vertices[vertexIndex2].position.x, mesh.vertices[vertexIndex2].position.y };

	const Vector2 edge01 = v1 - v0;
	const Vector2 edge12 = v2 - v1;
	const Vector2 edge20 = v0 - v2;

	const float areaTriangle = Vector2::Cross(v1 - v0, v2 - v0);

	ColorRGB finalColor{};

	const ColorRGB colorV0 = mesh.vertices[vertexIndex0].color;
	const ColorRGB colorV1 = mesh.vertices[vertexIndex2].color;
	const ColorRGB colorV2 = mesh.vertices[vertexIndex2].color;

	//bounding Box
	int minX = static_cast<int>(std::min({ v0.x, v1.x, v2.x }));
	int minY = static_cast<int>(std::min({ v0.y, v1.y, v2.y }));
	int maxX = static_cast<int>(std::max({ v0.x, v1.x, v2.x }));
	int maxY = static_cast<int>(std::max({ v0.y, v1.y, v2.y }));

	// Clamp bounding box within screen bounds
	minX = std::max(minX, 0);
	minY = std::max(minY, 0);
	maxX = std::min(maxX, m_Width - 1);
	maxY = std::min(maxY, m_Height - 1);

	for (int px{ minX }; px < maxX; ++px)
	{
		for (int py{ minY }; py < maxY; ++py)
		{
			Vector2 pixel = { static_cast<float>(px),static_cast<float>(py) };

			const Vector2 directionV0 = pixel - v0;
			const Vector2 directionV1 = pixel - v1;
			const Vector2 directionV2 = pixel - v2;

			//calc weights
			float weightV0 = Vector2::Cross(edge12, directionV1);
			float weightV1 = Vector2::Cross(edge20, directionV2);
			float weightV2 = Vector2::Cross(edge01, directionV0);


			if (weightV2 < 0)
				continue;
			if (weightV0 < 0)
				continue;
			if (weightV1 < 0)
				continue;

			weightV0 /= areaTriangle;
			weightV1 /= areaTriangle;
			weightV2 /= areaTriangle;

			const float depthWeight =
			{
				weightV0 * mesh.vertices[vertexIndex0].position.z +
				weightV1 * mesh.vertices[vertexIndex1].position.z +
				weightV2 * mesh.vertices[vertexIndex2].position.z
			};

			if (depthWeight > m_pDepthBufferPixels[px * m_Height + py])
				continue;

			m_pDepthBufferPixels[px * m_Height + py] = depthWeight;

			finalColor = colorV0 * weightV0 + colorV1 * weightV1 + colorV2 * weightV2;

			//Update Color in Buffer
			finalColor.MaxToOne();

			m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
				static_cast<uint8_t>(finalColor.r * 255),
				static_cast<uint8_t>(finalColor.g * 255),
				static_cast<uint8_t>(finalColor.b * 255));
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



