#pragma once
#include <cassert>
#include <iostream>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>

#include "Maths.h"
#include "Timer.h"

namespace dae
{
	struct Camera
	{
		Camera() = default;

		Camera(const Vector3& _origin, float _fovAngle):
			origin{_origin},
			fovAngle{_fovAngle}
		{
		}


		Vector3 origin{};
		float fovAngle{90.f};
		float aspectRatio{};
		float fov{ tanf((fovAngle * TO_RADIANS) / 2.f) };

		Vector3 forward{Vector3::UnitZ};
		Vector3 up{Vector3::UnitY};
		Vector3 right{Vector3::UnitX};

		float totalPitch{};
		float totalYaw{};

		Matrix invViewMatrix{};
		Matrix viewMatrix{};

		Matrix projectionMatrix{};

		float near{ .1f };
		float far{ 100.f };


		bool init{ false };
		void Initialize(float aR,float _fovAngle = 90.f, Vector3 _origin = {0.f,0.f,0.f})
		{
			fovAngle = _fovAngle;
			fov = tanf((fovAngle * TO_RADIANS) / 2.f);
			aspectRatio = aR;
			origin = _origin;
			init = true;
		}

		void CalculateViewMatrix()
		{
			//TODO W1
			right = Vector3::Cross(Vector3::UnitY, forward).Normalized();
			up = Vector3::Cross(forward, right);

			invViewMatrix = Matrix{ right,up,forward,origin };

			viewMatrix = invViewMatrix.Inverse();
			//ViewMatrix => Matrix::CreateLookAtLH(...) [not implemented yet]
			//DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixlookatlh
		}

		void CalculateProjectionMatrix(const float aspectRatio)
		{
			//TODO W3
			//ProjectionMatrix => Matrix::CreatePerspectiveFovLH(...) [not implemented yet]
			projectionMatrix = Matrix::CreatePerspectiveFovLH(fov, aspectRatio, near, far);
			//DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovlh
		}

		static bool IsOutsideFrustum(const Vector4& vector)
		{
			return (vector.x < -1.f || vector.x > 1.f || vector.y < -1.f || vector.y > 1.f || vector.z < -1.f || vector.z > 1.f);
		}

		void Update(Timer* pTimer)
		{
			const float deltaTime = pTimer->GetElapsed();

			//Camera Update Logic

			
			const float moveSpeed = 10.f;
			const float rotationSpeed = PI / 2.f;
		// Keyboard Input
			HandleKeyboardInput(deltaTime, moveSpeed);

			// Mouse Input
			HandleMouseInput(deltaTime, moveSpeed, rotationSpeed);

			Matrix finalRotation = Matrix::CreateRotationX(totalPitch) * Matrix::CreateRotationY(totalYaw);

			forward = finalRotation.TransformVector(Vector3::UnitZ);

			//Update Matrices
			CalculateViewMatrix();
			float ar = aspectRatio;
			float fv = fov;

			if (init ||!AreEqual(ar,aspectRatio)  ||!AreEqual(fv,fov) )
			{
				CalculateProjectionMatrix(aspectRatio); //Try to optimize this - should only be called once or when fov/aspectRatio changes
				init = false;
			}
		}

		void HandleKeyboardInput(float deltaTime, float moveSpeed)
		{
			const uint8_t* pKeyboardState = SDL_GetKeyboardState(nullptr);
			if (pKeyboardState[SDL_SCANCODE_W])
			{
				MoveForward(deltaTime, moveSpeed);
			}
			if (pKeyboardState[SDL_SCANCODE_S])
			{
				MoveBackward(deltaTime, moveSpeed);
			}
			if (pKeyboardState[SDL_SCANCODE_D])
			{
				origin.x += moveSpeed * deltaTime;
			}
			if (pKeyboardState[SDL_SCANCODE_A])
			{
				origin.x -= moveSpeed * deltaTime;
			}
			if (pKeyboardState[SDL_SCANCODE_Q])
			{
				MoveUp(deltaTime, moveSpeed);
			}
			if (pKeyboardState[SDL_SCANCODE_E])
			{
				MoveDown(deltaTime, moveSpeed);
			}
		}
		void HandleMouseInput(float deltaTime, float moveSpeed, float rotationSpeed)
		{
			int mouseX{}, mouseY{};
			const uint32_t mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);
			if (mouseState & SDL_BUTTON_LMASK && mouseState & SDL_BUTTON_RMASK)
			{
				if (mouseY < 0)
				{
					MoveUp(deltaTime, moveSpeed);
				}
				else if (mouseY > 0)
				{
					MoveDown(deltaTime, moveSpeed);
				}
			}
			else if (mouseState & SDL_BUTTON_RMASK)
			{
				if (mouseX != 0)
				{
					totalYaw += mouseX * rotationSpeed * deltaTime;
				}
				if (mouseY != 0)
				{
					totalPitch -= mouseY * rotationSpeed * deltaTime;
				}
			}
			//forward and backward translation with mouse
			else if (mouseState & SDL_BUTTON_LMASK)
			{
				if (mouseY < 0)
				{
					MoveForward(deltaTime, moveSpeed);
				}
				else if (mouseY > 0)
				{
					MoveBackward(deltaTime, moveSpeed);
				}
			}
		}

		void MoveForward(float deltaTime, float moveSpeed) { origin.z += deltaTime * moveSpeed; }
		void MoveBackward(float deltaTime, float moveSpeed) { origin.z -= deltaTime * moveSpeed; }
		void MoveUp(float deltaTime, float moveSpeed) { origin.y += deltaTime * moveSpeed; }
		void MoveDown(float deltaTime, float moveSpeed) { origin.y -= deltaTime * moveSpeed; }
	};
}
