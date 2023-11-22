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

	m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });

	//m_Ar = static_cast<float>(m_Width) / static_cast<float>(m_Height);
	
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
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

	std::vector<Vertex> vertices_ndc{};
	const std::vector<Vertex> vertices_world
	{

		{{0.f,2.f,0.f}},
		{{1.f,0.f,0.f}},
		{{-1.f,0.f,0.f}},
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

		const float fullTriangleArea{ Vector2::Cross(edge01, edge12) };

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

		for (int px{minX}; px < maxX; ++px)
		{
			for (int py{minY}; py < maxY; ++py)
			{

				ColorRGB finalColor{0,0,0};
				const int pixelIdx{ px + py * m_Width };
				const Vector2 curPixel{ static_cast<float>(px), static_cast<float>(py) };

				// Calculate the vector between the first vertex and the point
				const Vector2 directionV0{ curPixel - v0 };
				const Vector2 directionV1{ curPixel - v1 };
				const Vector2 directionV2{ curPixel - v2 };

				if(!(Vector2::Cross(edge01, directionV0) > 0) && (Vector2::Cross(edge12, directionV1) > 0) && (Vector2::Cross(edge20, directionV2) > 0))
					continue;


				// Calculate the barycentric weights
				const float weightV0{ Vector2::Cross(edge01, directionV0) / fullTriangleArea };
				const float weightV1{ Vector2::Cross(edge12, directionV1) / fullTriangleArea };
				const float weightV2{ Vector2::Cross(edge20, directionV2) / fullTriangleArea };


				//Calculate the depth
				const float depthV0{ (vertices_ndc[i].position.z) };
				const float depthV1{ (vertices_ndc[i + 1].position.z) };
				const float depthV2{ (vertices_ndc[i + 2].position.z) };
				



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
				finalColor = vertices_world[i].color;
				//Update Color in Buffer
				finalColor.MaxToOne();

				m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
					static_cast<uint8_t>(finalColor.r * 255),
					static_cast<uint8_t>(finalColor.g * 255),
					static_cast<uint8_t>(finalColor.b * 255));
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



