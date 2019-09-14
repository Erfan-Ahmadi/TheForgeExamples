#pragma once
#include "Common_3/OS/Interfaces/ICameraController.h"
#include "Common_3/OS/Interfaces/IApp.h"
#include "Common_3/OS/Interfaces/ILog.h"
#include "Common_3/OS/Interfaces/IInput.h"
#include "Common_3/OS/Interfaces/IFileSystem.h"
#include "Common_3/OS/Interfaces/ITime.h"
#include "Common_3/OS/Interfaces/IProfiler.h"
#include "Middleware_3/UI/AppUI.h"
#include "Common_3/Renderer/IRenderer.h"
#include "Common_3/Renderer/ResourceLoader.h"

//Math
#include "Common_3/OS/Math/MathTypes.h"

namespace meshes
{
	// No Indexing
	inline void generateQuadPoints(float** ppQuadPoints, uint32_t* pNumPoints)
	{
		const size_t num_points = 6 * 2;
		float3* pPoints = (float3*)conf_malloc(num_points * sizeof(float3));

		Vector3 topLeft		= Vector3{ -0.5f, +0.5f, +1.0f };
		Vector3 topRight	= Vector3{ +0.5f, +0.5f, +1.0f };
		Vector3 botLeft		= Vector3{ -0.5f, -0.5f, +1.0f };
		Vector3 botRight	= Vector3{ +0.5f, -0.5f, +1.0f };

		// Top Right Triangle
		pPoints[0] = v3ToF3(topRight);
		pPoints[1] = float3(0.0f, 0.0f, -1.0f);
		pPoints[2] = v3ToF3(botRight);
		pPoints[3] = float3(0.0f, 0.0f, -1.0f);
		pPoints[4] = v3ToF3(topLeft);
		pPoints[5] = float3(0.0f, 0.0f, -1.0f);

		// Bot Left Triangle
		pPoints[6] = v3ToF3(topLeft);
		pPoints[7] = float3(0.0f, 0.0f, -1.0f);
		pPoints[8] = v3ToF3(botRight);
		pPoints[9] = float3(0.0f, 0.0f, -1.0f);
		pPoints[10] = v3ToF3(botLeft);
		pPoints[11] = float3(0.0f, 0.0f, -1.0f);

		*pNumPoints = num_points;
		(*ppQuadPoints) = (float*)pPoints;
	}

	// Indexing
	inline void generateQuadPoints(float** ppQuadPoints, uint32_t* pNumPoints, uint32_t** ppIndices, uint32_t* pNumIndices)
	{
		const size_t num_points = 4 * 2;
		const size_t num_indices = 6;
		float3* pPoints = (float3*)conf_malloc(num_points * sizeof(float3));

		Vector3 topLeft		= Vector3{ -0.5f, +0.5f, -1.0f };
		Vector3 topRight	= Vector3{ +0.5f, +0.5f, -1.0f };
		Vector3 botLeft		= Vector3{ -0.5f, -0.5f, -1.0f };
		Vector3 botRight	= Vector3{ +0.5f, -0.5f, -1.0f };

		pPoints[0] = v3ToF3(topLeft);
		pPoints[1] = float3(0.0f, 0.0f, -1.0f);
		pPoints[2] = v3ToF3(botRight);
		pPoints[3] = float3(0.0f, 0.0f, -1.0f);
		pPoints[4] = v3ToF3(topRight);
		pPoints[5] = float3(0.0f, 0.0f, -1.0f);
		pPoints[6] = v3ToF3(botLeft);
		pPoints[7] = float3(0.0f, 0.0f, -1.0f);

		uint32_t pIndices[num_indices] = { 2, 1, 0, 0, 1, 3 };


		*pNumPoints = num_points;
		(*ppQuadPoints) = (float*)pPoints;
		(*pNumIndices) = num_indices;
		(*ppIndices) = pIndices;
	}
}