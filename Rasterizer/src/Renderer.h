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

		void VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const;

		void VertexTransformationToScreenSpace(const std::vector<Vertex_Out>& vertices_in, std::vector<Vector2>& vertex_out) const;

		void VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex_Out>& vertices_out, const Matrix& worldViewProjectionMatrix, const Matrix& meshWorldMatrix) const;

		void RenderTriangle() const;
		void RenderTriangleStrip(std::vector<Mesh>& meshes_world, std::vector<Vertex_Out>& vertices_ndc,std::vector<Vector2>&vertices_screen) const;

		ColorRGB PixelShading( Vertex_Out& v) const;

		void RotateMesh(float elapsedSec);
		void ClearBackground() const;
		void ResetDepthBuffer() const;

		enum class DisplayMode
		{
			depthBuffer,
			finalColor
		};
		enum class ShadingMode
		{
			observed,
			diffuse,
			specular,
			combined
		};
		enum class State
		{
			rotate,
			idle
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

		std::vector<Mesh>m_MeshesWorld;
		Matrix m_MeshOriginalWorldMatrix{};
		Texture* m_pTexture{};
		Texture* m_pNormalMap{};
		Texture* m_pGlossinessMap{};
		Texture* m_pSpecularMap{};

		bool m_DepthBuffer{false};
		bool m_UseNormalMap{ false };
		bool m_RotateMesh{ false };
		float m_MeshRotationAngle{ PI_DIV_2 };

		void HandleKeyInput();
		DisplayMode m_displayMode{ DisplayMode::finalColor };
		ShadingMode m_ShadingMode{ ShadingMode::combined };
		State m_State{ State::idle };
	};
}
