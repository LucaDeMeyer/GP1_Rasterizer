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

		for (int px{minX}; px < maxX; ++px)
		{
			for (int py{minY}; py < maxY; ++py)
			{

				ColorRGB finalColor{0,0,0};
			
				const Vector2 pixel{ static_cast<float>(px),static_cast<float>(py)};

				// Calculate the vector between vertex and the point
				const Vector2 directionV0{ pixel - v0 };
				const Vector2 directionV1{ pixel - v1 };
				const Vector2 directionV2{ pixel - v2 };

			
				


				// Calculate the barycentric weights
				 float weightV0{ Vector2::Cross(edge12 , directionV1) };
				 float weightV1{ Vector2::Cross(edge20,directionV2) };
				 float weightV2{ Vector2::Cross(edge01,directionV0) };

				//hit-test
				if(weightV0 < 0)
					continue;
				if(weightV1< 0)
					continue;
				if(weightV2 < 0)
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



