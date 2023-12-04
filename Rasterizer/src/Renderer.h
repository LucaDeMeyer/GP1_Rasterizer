#pragma once

#include <cstdint>
#include <vector>

#include "Camera.h"

struct SDL_Window;
struct SDL_Surface;

namespace dae
{
	struct Vertex_Out;
	class Texture;
	struct Mesh;
	struct Vertex;
	class Timer;
	class Scene;

	class Renderer final
	{
	public:
		Renderer(SDL_Window* pWindow);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) noexcept = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) noexcept = delete;

		void Update(Timer* pTimer);
		void Render() const;
			

		bool SaveBufferToImage() const;

		//void VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const;

		void VertexTransformationToScreenSpace(const std::vector<Vertex_Out>& vertices_in, std::vector<Vector2>& vertex_out) const;

		void VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex_Out>& vertices_out, const Matrix& worldViewProjectionMatrix, const Matrix& meshWorldMatrix) const;

		void RenderTriangle() const;
		void ClearBackground() const;
		void ResetDepthBuffer() const;

		enum class DisplayMode
		{
			depthBuffer,
			finalColor
		};
	private:
		SDL_Window* m_pWindow{};

		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};

		float* m_pDepthBufferPixels{};

		Camera m_Camera{};

		int m_Width{};
		int m_Height{};
		float m_Ar{};
		int m_FOV{ 90 };

		Texture* m_pTexture{};

		bool m_DepthBuffer{false};

		void HandleKeyInput();
		DisplayMode m_displayMode{ DisplayMode::finalColor };
	};
}
