#include "../common.h"

#include "Common_3/OS/Interfaces/IProfiler.h"
#include "Middleware_3/UI/AppUI.h"

//EASTL includes
#include "Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "Common_3/ThirdParty/OpenSource/EASTL/string.h"

//asimp importer
#include "Common_3/Tools/AssimpImporter/AssimpImporter.h"
#include "Common_3/OS/Interfaces/IMemory.h"

constexpr size_t gDirectionalLights = 1;
constexpr size_t gMaxDirectionalLights = 1;
static_assert(gDirectionalLights <= gMaxDirectionalLights, "");

constexpr size_t gPointLights = 1;
constexpr size_t gMaxPointLights = 8;
static_assert(gPointLights <= gMaxPointLights, "");

constexpr size_t gSpotLights = 1;
constexpr size_t gMaxSpotLights = 8;
static_assert(gSpotLights <= gMaxSpotLights, "");

const uint32_t	gImageCount = 3;

Renderer* pRenderer = NULL;

Queue* pGraphicsQueue = NULL;

Fence* pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };
Semaphore* pImageAquiredSemaphore = NULL;
Semaphore* pFirstPassFinishedSemaphore = { NULL };

Sampler* pSampler;

SwapChain* pSwapChain = NULL;

uint32_t			gFrameIndex = 0;

DescriptorBinder* pDescriptorBinder = NULL;

bool           gMicroProfiler = false;
bool           bPrevToggleMicroProfiler = false;

UIApp gAppUI;
GuiComponent* pGui = NULL;
VirtualJoystickUI gVirtualJoystick;
GpuProfiler* pGpuProfiler = NULL;
ICameraController* pCameraController = NULL;
TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

RenderTarget* pDepthBuffer;

RasterizerState* pRasterDefault = NULL;
DepthState* pDepthDefault = NULL;
DepthState* pDepthNone = NULL;

struct
{
	Texture* pTextureColor = NULL;
	Texture* pTextureSpecular = NULL;
} textures;

const char* pTexturesFileNames[] =
{
	"lion/lion_albedo",
	"lion/lion_specular"
};

//Enum for easy access of GBuffer RTs
struct GBufferRT
{
	enum Enum
	{
		AlbedoSpecular,
		Normal,
		Position,
		COUNT
	};
};

class RenderPassData
{
public:
	Shader* pShader;
	RootSignature* pRootSignature;
	Pipeline* pPipeline;
	CmdPool* pCmdPool;
	Cmd** ppCmds;
	Buffer* pPerPassCB[gImageCount];
	eastl::vector<RenderTarget*> RenderTargets;
	eastl::vector<Texture*>      Textures;

	RenderPassData(Renderer* pRenderer, Queue* bGraphicsQueue, int ImageCount)
	{
		addCmdPool(pRenderer, bGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, ImageCount, &ppCmds);
	}
};

struct RenderPass
{
	enum Enum
	{
		GPass,
		Deffered
	};
};

typedef eastl::unordered_map<RenderPass::Enum, RenderPassData*> RenderPassMap;

struct MeshBatch
{
	Buffer* pPositionStream;
	Buffer* pNormalStream;
	Buffer* pUVStream;
	Buffer* pIndicesStream;
	size_t mCountIndices;
};

struct SceneData
{
	eastl::vector<MeshBatch*> meshes;
} sceneData;

struct DirectionalLight
{
#ifdef VULKAN
	alignas(16) float3 direction;
	alignas(16) float3 ambient;
	alignas(16) float3 diffuse;
	alignas(16) float3 specular;
#elif DIRECT3D12
	float3 direction;
	float3 ambient;
	float3 diffuse;
	float3 specular;
#endif
};

struct PointLight
{
#ifdef VULKAN
	alignas(16) float3 position;
	alignas(16) float3 ambient;
	alignas(16) float3 diffuse;
	alignas(16) float3 specular;
	alignas(16) float3 attenuationParams;
#elif DIRECT3D12
	float3 position;
	float3 ambient;
	float3 diffuse;
	float3 specular;
	float3 attenuationParams;
	float _pad0;
#endif
};

struct SpotLight
{
#ifdef VULKAN
	alignas(16) float3 position;
	alignas(16) float3 direction;
	alignas(16) float2 cutOffs;
	alignas(16) float3 ambient;
	alignas(16) float3 diffuse;
	alignas(16) float3 specular;
	alignas(16) float3 attenuationParams;
#elif DIRECT3D12
	float3 position;
	float3 direction;
	float2 cutOffs;
	float3 ambient;
	float3 diffuse;
	float3 specular;
	float3 attenuationParams;
#endif
};

struct
{
	int numDirectionalLights;
	int numPointLights;
	int numSpotLights;
	alignas(16) float3 viewPos;
} lightData;

DirectionalLight	directionalLights[gDirectionalLights];
PointLight			pointLights[gPointLights];
SpotLight			spotLights[gSpotLights];

struct
{
	Buffer* pDirLightsBuffer = NULL;
	Buffer* pPointLightsBuffer = NULL;
	Buffer* pSpotLightsBuffer = NULL;
} lightBuffers;

struct
{
	Buffer* vertexBuffer;
	Buffer* indexBuffer;
	size_t indexCount;
	size_t vertexCount;
} quads;

Buffer* pLightBuffer = NULL;

struct
{
	mat4	view;
	mat4	proj;
	mat4	pToWorld;
} gOffscreenUniformData;

Buffer* pOffscreenUBOs[gImageCount] = { NULL };

bool debugDisplay = 1.0f;
struct
{
	mat4 proj;
} gDefferedUniformData;

Buffer* pDefferedUBOs[gImageCount] = { NULL };

RenderPassMap	RenderPasses;

const char* pszBases[FSR_Count] = {
	"../../../../src/Shaders/bin",													// FSR_BinShaders
	"../../../../src/07_DefferedLighting/",											// FSR_SrcShaders
	"../../../../art/",																// FSR_Textures
	"../../../../art/",																// FSR_Meshes
	"../../../../../The-Forge/Examples_3/Unit_Tests/UnitTestResources/",			// FSR_Builtin_Fonts
	"../../../../../The-Forge/Examples_3/Unit_Tests/src/01_Transformations/",		// FSR_GpuConfig
	"",																				// FSR_Animation
	"",																				// FSR_Audio
	"",																				// FSR_OtherFiles
	"../../../../../The-Forge/Middleware_3/Text/",									// FSR_MIDDLEWARE_TEXT
	"../../../../../The-Forge/Middleware_3/UI/",									// FSR_MIDDLEWARE_UI
};

class DefferedLighting : public IApp
{
public:
	DefferedLighting()
	{
	}

	bool Init()
	{
		// window and render setup
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		// check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		queueDesc.mFlag = QUEUE_FLAG_NONE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		//Gbuffer pass
		RenderPassData* pass =
			conf_placement_new<RenderPassData>(conf_calloc(1, sizeof(RenderPassData)), pRenderer, pGraphicsQueue, gImageCount);
		RenderPasses.insert(eastl::pair<RenderPass::Enum, RenderPassData*>(RenderPass::GPass, pass));

		//Shadow pass
		pass =
			conf_placement_new<RenderPassData>(conf_calloc(1, sizeof(RenderPassData)), pRenderer, pGraphicsQueue, gImageCount);
		RenderPasses.insert(eastl::pair<RenderPass::Enum, RenderPassData*>(RenderPass::Deffered, pass));

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAquiredSemaphore);
		addSemaphore(pRenderer, &pFirstPassFinishedSemaphore);

		// Resource Loading
		initResourceLoaderInterface(pRenderer);

		// Initialize profile
		initProfiler(pRenderer);

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

		// Shader
		ShaderLoadDesc shaderDesc = {};
		shaderDesc.mStages[0] = { "offscreen.vert", NULL, 0, FSR_SrcShaders };
		shaderDesc.mStages[1] = { "offscreen.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &shaderDesc, &RenderPasses[RenderPass::GPass]->pShader);
		shaderDesc.mStages[0] = { "deffered.vert", NULL, 0, FSR_SrcShaders };
		shaderDesc.mStages[1] = { "deffered.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &shaderDesc, &RenderPasses[RenderPass::Deffered]->pShader);

		//Create rasteriser state objects
		{
			RasterizerStateDesc rasterizerStateDesc = {};
			rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
			addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterDefault);
		}

		//Create depth state objects
		{
			DepthStateDesc depthStateDesc = {};
			depthStateDesc.mDepthTest = true;
			depthStateDesc.mDepthWrite = true;
			depthStateDesc.mDepthFunc = CMP_LEQUAL;
			addDepthState(pRenderer, &depthStateDesc, &pDepthDefault);

			depthStateDesc.mDepthTest = false;
			depthStateDesc.mDepthWrite = false;
			depthStateDesc.mDepthFunc = CMP_ALWAYS;
			addDepthState(pRenderer, &depthStateDesc, &pDepthNone);
		}

		// Static Samplers
		SamplerDesc samplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSampler);

		// Resource Binding
		const char* samplerNames = { "uSampler0 " };

		{
			// Root Signature for Offscreen Pipeline
			{
				RootSignatureDesc rootDesc = {};
				rootDesc.mShaderCount = 1;
				rootDesc.ppShaders = &RenderPasses[RenderPass::GPass]->pShader;
				rootDesc.mStaticSamplerCount = 1;
				rootDesc.ppStaticSamplerNames = &samplerNames;
				rootDesc.ppStaticSamplers = &pSampler;
				addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::GPass]->pRootSignature);
			}

			// Root Signature for Deffered Pipeline
			{
				RootSignatureDesc rootDesc = {};
				rootDesc.mShaderCount = 1;
				rootDesc.ppShaders = &RenderPasses[RenderPass::Deffered]->pShader;
				rootDesc.mStaticSamplerCount = 1;
				rootDesc.ppStaticSamplerNames = &samplerNames;
				rootDesc.ppStaticSamplers = &pSampler;
				addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::Deffered]->pRootSignature);
			}
		}

		DescriptorBinderDesc descriptorBinderDescs[2] =
		{
			{ RenderPasses[RenderPass::GPass]->pRootSignature },
			{ RenderPasses[RenderPass::Deffered]->pRootSignature }
		};

		addDescriptorBinder(pRenderer, 0, 2, descriptorBinderDescs, &pDescriptorBinder);

		// Create Uniform Buffers
		{
			// Offscreen

			{
				BufferLoadDesc bufferDesc = {};
				bufferDesc = {};
				bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				bufferDesc.mDesc.mSize = sizeof(gOffscreenUniformData);
				bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				bufferDesc.pData = NULL;

				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					bufferDesc.ppBuffer = &pOffscreenUBOs[i];
					addResource(&bufferDesc);
				}
			}

			// Deffered
			{
				BufferLoadDesc bufferDesc = {};
				bufferDesc = {};
				bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				bufferDesc.mDesc.mSize = sizeof(gDefferedUniformData);
				bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				bufferDesc.pData = NULL;

				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					bufferDesc.ppBuffer = &pDefferedUBOs[i];
					addResource(&bufferDesc);
				}
			}
		}

		CreateLightsBuffer();
		GenerateQuads();

		if (!LoadModels())
		{
			finishResourceLoading();
			return false;
		}

		finishResourceLoading();

		// GUI
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		GuiDesc guiDesc = {};
		float   dpiScale = getDpiScale().x;
		guiDesc.mStartSize = vec2(140.0f, 320.0f);
		guiDesc.mStartPosition = vec2(mSettings.mWidth / dpiScale - guiDesc.mStartSize.getX() * 1.1f, guiDesc.mStartSize.getY() * 0.5f);

		pGui = gAppUI.AddGuiComponent("Micro profiler", &guiDesc);

		pGui->AddWidget(CheckboxWidget("Toggle Micro Profiler", &gMicroProfiler));

		// Camera
		CameraMotionParameters cmp{ 40.0f, 30.0f, 100.0f };
		vec3                   camPos{ 0.0f, 0.0f, 0.8f };
		vec3                   lookAt{ 0.0f, 0.0f, 5.0f };

		pCameraController = createFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);
		if (!initInputSystem(pWindow))
			return false;

		// Microprofiler Actions
		// #TODO: Remove this once the profiler UI is ported to use our UI system
		InputActionDesc actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { onProfilerButton(false, &ctx->mFloat2, true); return !gMicroProfiler; } };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_SOUTH, [](InputActionContext* ctx) { onProfilerButton(ctx->mBool, ctx->pPosition, false); return true; } };
		addInputAction(&actionDesc);

		// App Actions
		actionDesc = { InputBindings::BUTTON_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
		addInputAction(&actionDesc);

		actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
		addInputAction(&actionDesc);

		actionDesc =
		{
			InputBindings::BUTTON_ANY, [](InputActionContext* ctx)
			{
				bool capture = gAppUI.OnButton(ctx->mBinding, ctx->mBool, ctx->pPosition, !gMicroProfiler);
				setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
				return true;
			}, this
		};
		addInputAction(&actionDesc);

		typedef bool (*CameraInputHandler)(InputActionContext * ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (!gMicroProfiler && !gAppUI.IsFocused() && *ctx->pCaptured)
			{
				gVirtualJoystick.OnMove(index, ctx->mPhase != INPUT_ACTION_PHASE_CANCELED, ctx->pPosition);
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			}
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 0.5f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);

		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

		destroyCameraController(pCameraController);

		gVirtualJoystick.Exit();

		gAppUI.Exit();

		// Exit profile
		exitProfiler();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pOffscreenUBOs[i]);
		}

		//Delete rendering passes
		for (RenderPassMap::iterator iter = RenderPasses.begin(); iter != RenderPasses.end(); ++iter)
		{
			RenderPassData* pass = iter->second;

			if (!pass)
				continue;

			for (RenderTarget* rt : pass->RenderTargets)
			{
				removeRenderTarget(pRenderer, rt);
			}

			for (Texture* texture : pass->Textures)
			{
				removeResource(texture);
			}

			for (uint32_t j = 0; j < gImageCount; ++j)
			{
				if (pass->pPerPassCB[j])
					removeResource(pass->pPerPassCB[j]);
			}

			removeCmd_n(pass->pCmdPool, gImageCount, pass->ppCmds);
			removeCmdPool(pRenderer, pass->pCmdPool);

			removeShader(pRenderer, pass->pShader);

			removeRootSignature(pRenderer, pass->pRootSignature);
			pass->~RenderPassData();
			conf_free(pass);
		}

		RenderPasses.empty();

		for (size_t i = 0; i < sceneData.meshes.size(); ++i)
		{
			removeResource(sceneData.meshes[i]->pPositionStream);
			removeResource(sceneData.meshes[i]->pUVStream);
			removeResource(sceneData.meshes[i]->pNormalStream);
			removeResource(sceneData.meshes[i]->pIndicesStream);
		}

		removeDescriptorBinder(pRenderer, pDescriptorBinder);

		removeRasterizerState(pRasterDefault);
		removeDepthState(pDepthDefault);
		removeDepthState(pDepthNone);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAquiredSemaphore);

		removeSemaphore(pRenderer, pFirstPassFinishedSemaphore);

		removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		CreateRenderTargets();

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;

		loadProfiler(pSwapChain->ppSwapchainRenderTargets[0]);

		CreatePipelines();

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		unloadProfiler();
		gAppUI.Unload();

		gVirtualJoystick.Unload();

		//Delete rendering passes Textures and Render targets
		for (RenderPassMap::iterator iter = RenderPasses.begin(); iter != RenderPasses.end(); ++iter)
		{
			RenderPassData* pass = iter->second;

			for (RenderTarget* rt : pass->RenderTargets)
			{
				removeRenderTarget(pRenderer, rt);
			}

			for (Texture* texture : pass->Textures)
			{
				removeResource(texture);
			}
			pass->RenderTargets.clear();
			pass->Textures.clear();

			if (pass->pPipeline)
			{
				removePipeline(pRenderer, pass->pPipeline);
				pass->pPipeline = NULL;
			}
		}

		removeRenderTarget(pRenderer, pDepthBuffer);
		removeSwapChain(pRenderer, pSwapChain);
		pDepthBuffer = NULL;
		pSwapChain = NULL;
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);
		pCameraController->update(deltaTime);

		static float currentTime;
		currentTime += deltaTime;

		// update camera with time
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

		gOffscreenUniformData.view = viewMat;
		gOffscreenUniformData.proj = projMat;

		// Update Instance Data
		gOffscreenUniformData.pToWorld = mat4::translation(Vector3(0.0f, -1, 7)) *
			mat4::rotationY(currentTime) *
			mat4::scale(Vector3(0.1f));

		viewMat.setTranslation(vec3(0));


		directionalLights[0].direction = float3{ 0.0f, -1.0f, 0.0f };
		directionalLights[0].ambient = float3{ 0.05f, 0.05f, 0.05f };
		directionalLights[0].diffuse = float3{ 0.5f, 0.5f, 0.5f };
		directionalLights[0].specular = float3{ 0.5f, 0.5f, 0.5f };
		lightData.numDirectionalLights = gDirectionalLights;

		pointLights[0].position = float3{ -4.0f, 4.0f, 5.0f };
		pointLights[0].ambient = float3{ 0.1f, 0.1f, 0.1f };
		pointLights[0].diffuse = float3{ 1.0f, 1.0f, 1.0f };
		pointLights[0].specular = float3{ 1.0f, 1.0f, 1.0f };
		pointLights[0].attenuationParams = float3{ 1.0f, 0.07f, 0.017f };
		lightData.numPointLights = gPointLights;

		spotLights[0].position = float3{ 4.0f * sin(currentTime), 0.0f, 0.0f };
		spotLights[0].direction = float3{ -0.5f, 0.0f, 1.0f };
		spotLights[0].cutOffs = float2{ cos(PI / 9), cos(PI / 6) };
		spotLights[0].ambient = float3{ 0.1f, 0.1f, 0.1f };
		spotLights[0].diffuse = float3{ 1.0f, 1.0f, 1.0f };
		spotLights[0].specular = float3{ 1.0f, 1.0f, 1.0f };
		spotLights[0].attenuationParams = float3{ 1.0f, 0.07f, 0.017f };
		lightData.numSpotLights = gSpotLights;

		lightData.viewPos = v3ToF3(pCameraController->getViewPosition());

		if (debugDisplay)
		{
			gDefferedUniformData.proj = mat4::orthographic(0.0f, 2.0f, 0.0f, 2.0f, -1.0f, 1.0f);
		}
		else
		{
			gDefferedUniformData.proj = mat4::orthographic(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
		}

		if (gMicroProfiler != bPrevToggleMicroProfiler)
		{
			Profile& S = *ProfileGet();
			int nValue = gMicroProfiler ? 1 : 0;
			nValue = nValue >= 0 && nValue < P_DRAW_SIZE ? nValue : S.nDisplay;
			S.nDisplay = nValue;

			bPrevToggleMicroProfiler = gMicroProfiler;
		}

		/************************************************************************/
		// Update GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAquiredSemaphore, NULL, &gFrameIndex);

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Update uniform buffers
		BufferUpdateDesc viewProjCbv = { pOffscreenUBOs[gFrameIndex], &gOffscreenUniformData };
		updateResource(&viewProjCbv);
		viewProjCbv = { pDefferedUBOs[gFrameIndex], &gDefferedUniformData };
		updateResource(&viewProjCbv);

		// Update light uniform buffers
		BufferUpdateDesc lightBuffUpdate = { pLightBuffer, &lightData };
		updateResource(&lightBuffUpdate);

		BufferUpdateDesc pointLightBuffUpdate = { lightBuffers.pPointLightsBuffer, &pointLights };
		updateResource(&pointLightBuffUpdate);

		BufferUpdateDesc dirLightBuffUpdate = { lightBuffers.pDirLightsBuffer, &directionalLights };
		updateResource(&dirLightBuffUpdate);

		BufferUpdateDesc spotLightBuffUpdate = { lightBuffers.pSpotLightsBuffer, &spotLights };
		updateResource(&spotLightBuffUpdate);

		// Load Actions
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 0.0f;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;

		// Offscreen Pass
		Cmd* cmd = RenderPasses[RenderPass::GPass]->ppCmds[gFrameIndex];
		{
			beginCmd(cmd);
			cmdBeginGpuFrameProfile(cmd, pGpuProfiler);
			{
				cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Offscreen Pass", true);

				// Clear G-buffers and Depth buffer
				LoadActionsDesc loadActions = {};
				for (uint32_t i = 0; i < RenderPasses[RenderPass::GPass]->RenderTargets.size(); ++i)
				{
					loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
					loadActions.mClearColorValues[i] = RenderPasses[RenderPass::GPass]->RenderTargets[i]->mDesc.mClearValue;
				}

				// Clear depth to the far plane and stencil to 0
				loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
				loadActions.mClearDepth = { 1.0f, 0.0f };

				// Transfer G-buffers to render target state for each buffer
				TextureBarrier barrier;
				for (uint32_t i = 0; i < RenderPasses[RenderPass::GPass]->RenderTargets.size(); ++i)
				{
					barrier = { RenderPasses[RenderPass::GPass]->RenderTargets[i]->pTexture, RESOURCE_STATE_RENDER_TARGET };
					cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
				}

				// Transfer DepthBuffer to a DephtWrite State
				barrier = { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE };
				cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

				cmdBindRenderTargets(
					cmd,
					(uint32_t)RenderPasses[RenderPass::GPass]->RenderTargets.size(),
					RenderPasses[RenderPass::GPass]->RenderTargets.data(),
					pDepthBuffer,
					&loadActions, NULL, NULL, -1, -1);

				cmdSetViewport(
					cmd, 0.0f, 0.0f, (float)RenderPasses[RenderPass::GPass]->RenderTargets[0]->mDesc.mWidth,
					(float)RenderPasses[RenderPass::GPass]->RenderTargets[0]->mDesc.mHeight, 0.0f, 1.0f);
				cmdSetScissor(
					cmd, 0, 0, RenderPasses[RenderPass::GPass]->RenderTargets[0]->mDesc.mWidth,
					RenderPasses[RenderPass::GPass]->RenderTargets[0]->mDesc.mHeight);

				{
					DescriptorData params[3] = {};
					params[0].pName = "Texture";
					params[0].ppTextures = &textures.pTextureColor;
					params[1].pName = "TextureSpecular";
					params[1].ppTextures = &textures.pTextureSpecular;
					params[2].pName = "UniformData";
					params[2].ppBuffers = &pOffscreenUBOs[gFrameIndex];

					cmdBindPipeline(cmd, RenderPasses[RenderPass::GPass]->pPipeline);
					{
						cmdBindDescriptors(cmd, pDescriptorBinder, RenderPasses[RenderPass::GPass]->pRootSignature, 3, params);
						Buffer* pVertexBuffers[] = { sceneData.meshes[0]->pPositionStream, sceneData.meshes[0]->pNormalStream, sceneData.meshes[0]->pUVStream };
						cmdBindVertexBuffer(cmd, 3, pVertexBuffers, NULL);

						cmdBindIndexBuffer(cmd, sceneData.meshes[0]->pIndicesStream, 0);

						cmdDrawIndexed(cmd, sceneData.meshes[0]->mCountIndices, 0, 0);
					}
				}

				cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
			}
			endCmd(cmd);
		}

		queueSubmit(pGraphicsQueue, 1, &cmd, NULL, 1, &pImageAquiredSemaphore, 1, &pFirstPassFinishedSemaphore);

		// Deffered Pass
		cmd = RenderPasses[RenderPass::Deffered]->ppCmds[gFrameIndex];
		{
			RenderTarget* pSwapChainRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

			beginCmd(cmd);
			{
				cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Deffered Pass", true);

				TextureBarrier textureBarriers[5] = {
					{ pSwapChainRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
					{ RenderPasses[RenderPass::GPass]->RenderTargets[GBufferRT::AlbedoSpecular]->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
					{ RenderPasses[RenderPass::GPass]->RenderTargets[GBufferRT::Normal]->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
					{ RenderPasses[RenderPass::GPass]->RenderTargets[GBufferRT::Position]->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
					{ pDepthBuffer->pTexture, RESOURCE_STATE_SHADER_RESOURCE }
				};

				cmdResourceBarrier(cmd, 0, nullptr, 3, textureBarriers, false);

				loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
				cmdBindRenderTargets(cmd, 1, &pSwapChainRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
				cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSwapChainRenderTarget->mDesc.mWidth, (float)pSwapChainRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, pSwapChainRenderTarget->mDesc.mWidth, pSwapChainRenderTarget->mDesc.mHeight);

				{
					DescriptorData params[9] = {};
					params[0].pName = "depthBuffer";
					params[0].ppTextures = &pDepthBuffer->pTexture;
					params[1].pName = "albedoSpec";
					params[1].ppTextures = &RenderPasses[RenderPass::GPass]->RenderTargets[GBufferRT::AlbedoSpecular]->pTexture;
					params[2].pName = "normal";
					params[2].ppTextures = &RenderPasses[RenderPass::GPass]->RenderTargets[GBufferRT::Normal]->pTexture;
					params[3].pName = "position";
					params[3].ppTextures = &RenderPasses[RenderPass::GPass]->RenderTargets[GBufferRT::Position]->pTexture;
					params[4].pName = "LightData";
					params[4].ppBuffers = &pLightBuffer;
					params[5].pName = "DirectionalLights";
					params[5].ppBuffers = &lightBuffers.pDirLightsBuffer;
					params[6].pName = "PointLights";
					params[6].ppBuffers = &lightBuffers.pPointLightsBuffer;
					params[7].pName = "SpotLights";
					params[7].ppBuffers = &lightBuffers.pSpotLightsBuffer;
					params[8].pName = "uniformData";
					params[8].ppBuffers = &pDefferedUBOs[gFrameIndex];

					cmdBindPipeline(cmd, RenderPasses[RenderPass::Deffered]->pPipeline);
					{
						cmdBindDescriptors(cmd, pDescriptorBinder, RenderPasses[RenderPass::Deffered]->pRootSignature, 9, params);

						Buffer* pVertexBuffers[] = { quads.vertexBuffer };
						cmdBindVertexBuffer(cmd, 1, pVertexBuffers, NULL);
						cmdBindIndexBuffer(cmd, quads.indexBuffer, 0);

						// Draw Fullscreen Quad
						cmdDrawIndexed(cmd, 6, 0, 0);
					}
				}

				cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

				loadActions = {};
				loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
				cmdBindRenderTargets(cmd, 1, &pSwapChainRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
				cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw UI", true);
				{
					static HiresTimer gTimer;
					gTimer.GetUSec(true);

					gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

					gAppUI.DrawText(cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

#if !defined(__ANDROID__)
					gAppUI.DrawText(
						cmd, float2(8, 40), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
						&gFrameTimeDraw);
					gAppUI.DrawDebugGpuProfile(cmd, float2(8, 65), pGpuProfiler, NULL);
#endif

					gAppUI.Gui(pGui);

					cmdDrawProfiler(cmd);

					gAppUI.Draw(cmd);
					cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
				}
				cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

				textureBarriers[0] = { pSwapChainRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
				cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, true);
			}
			cmdEndGpuFrameProfile(cmd, pGpuProfiler);
			endCmd(cmd);
		}

		// Submit Second Pass Command Buffer
		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pFirstPassFinishedSemaphore, 1, &pRenderCompleteSemaphore);

		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);

		waitForFences(pRenderer, 1, &pRenderCompleteFence);

		flipProfiler();
	}

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mEnableVsync = true;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
		return pSwapChain != NULL;
	}

	void CreateRenderTargets()
	{
		ClearValue colorClearBlack = { 0.0f, 0.0f, 0.0f, 0.0f };

		// Offscreen Render Targets
		{
			RenderTarget* rendertarget;
			RenderTargetDesc rtDesc = {};
			rtDesc.mClearValue = colorClearBlack;
			rtDesc.mArraySize = 1;
			rtDesc.mDepth = 1;
			rtDesc.mWidth = mSettings.mWidth;
			rtDesc.mHeight = mSettings.mHeight;
			rtDesc.mSampleCount = SAMPLE_COUNT_1;
			rtDesc.mSampleQuality = 0;

			// Color + Specular
			rtDesc.mFormat = ImageFormat::RGBA8;
			::addRenderTarget(pRenderer, &rtDesc, &rendertarget);
			RenderPasses[RenderPass::GPass]->RenderTargets.push_back(rendertarget);

			// Normal
			rtDesc.mFormat = ImageFormat::RGBA8S;
			::addRenderTarget(pRenderer, &rtDesc, &rendertarget);
			RenderPasses[RenderPass::GPass]->RenderTargets.push_back(rendertarget);

			// Position
			rtDesc.mFormat = ImageFormat::RGBA8S;
			::addRenderTarget(pRenderer, &rtDesc, &rendertarget);
			RenderPasses[RenderPass::GPass]->RenderTargets.push_back(rendertarget);
		}

		// Depth Buffer
		{
			RenderTargetDesc rtDesc = {};
			rtDesc.mArraySize = 1;
			rtDesc.mClearValue.depth = 1.0f;
			rtDesc.mClearValue.stencil = 0.0f;
			rtDesc.mFormat = ImageFormat::D32F;
			rtDesc.mDepth = 1;
			rtDesc.mWidth = mSettings.mWidth;
			rtDesc.mHeight = mSettings.mHeight;
			rtDesc.mSampleCount = SAMPLE_COUNT_1;
			rtDesc.mSampleQuality = 0;
			::addRenderTarget(pRenderer, &rtDesc, &pDepthBuffer);
		}
	}

	void CreatePipelines()
	{
		// Offscreen
		{
			VertexLayout vertexLayout = {};
			vertexLayout.mAttribCount = 3;

			vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
			vertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
			vertexLayout.mAttribs[0].mBinding = 0;
			vertexLayout.mAttribs[0].mLocation = 0;
			vertexLayout.mAttribs[0].mOffset = 0;

			vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
			vertexLayout.mAttribs[1].mFormat = ImageFormat::RGB32F;
			vertexLayout.mAttribs[1].mBinding = 1;
			vertexLayout.mAttribs[1].mLocation = 1;
			vertexLayout.mAttribs[1].mOffset = 0;

			vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
			vertexLayout.mAttribs[2].mFormat = ImageFormat::RG32F;
			vertexLayout.mAttribs[2].mBinding = 2;
			vertexLayout.mAttribs[2].mLocation = 2;
			vertexLayout.mAttribs[2].mOffset = 0;

			//set up g-prepass buffer formats
			ImageFormat::Enum deferredFormats[GBufferRT::COUNT] = {};
			bool              deferredSrgb[GBufferRT::COUNT] = {};
			for (uint32_t i = 0; i < GBufferRT::COUNT; ++i)
			{
				deferredFormats[i] = RenderPasses[RenderPass::GPass]->RenderTargets[i]->mDesc.mFormat;
				deferredSrgb[i] = false;
			}

			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 3;
			pipelineSettings.pDepthState = pDepthDefault;
			pipelineSettings.pColorFormats = deferredFormats;
			pipelineSettings.pSrgbValues = deferredSrgb;
			pipelineSettings.mSampleCount = RenderPasses[RenderPass::GPass]->RenderTargets[0]->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = RenderPasses[RenderPass::GPass]->RenderTargets[0]->mDesc.mSampleQuality;
			pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
			pipelineSettings.pRootSignature = RenderPasses[RenderPass::GPass]->pRootSignature;
			pipelineSettings.pShaderProgram = RenderPasses[RenderPass::GPass]->pShader;
			pipelineSettings.pVertexLayout = &vertexLayout;
			pipelineSettings.pRasterizerState = pRasterDefault;

			addPipeline(pRenderer, &desc, &RenderPasses[RenderPass::GPass]->pPipeline);
		}

		// Deffered
		{
			VertexLayout vertexLayout = {};
			vertexLayout.mAttribCount = 2;

			vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
			vertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
			vertexLayout.mAttribs[0].mBinding = 0;
			vertexLayout.mAttribs[0].mLocation = 0;
			vertexLayout.mAttribs[0].mOffset = 0;

			vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
			vertexLayout.mAttribs[1].mFormat = ImageFormat::RG32F;
			vertexLayout.mAttribs[1].mBinding = 1;
			vertexLayout.mAttribs[1].mLocation = 1;
			vertexLayout.mAttribs[1].mOffset = 0;

			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = pDepthNone;
			pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
			pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
			pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
			pipelineSettings.pRootSignature = RenderPasses[RenderPass::Deffered]->pRootSignature;
			pipelineSettings.pShaderProgram = RenderPasses[RenderPass::Deffered]->pShader;
			pipelineSettings.pVertexLayout = &vertexLayout;
			pipelineSettings.pRasterizerState = pRasterDefault;

			addPipeline(pRenderer, &desc, &RenderPasses[RenderPass::Deffered]->pPipeline);
		}
	}

	const char* GetName() { return "07_DefferedLighting"; }

	void RecenterCameraView(float maxDistance, vec3 lookAt = vec3(0))
	{
		vec3 p = pCameraController->getViewPosition();
		vec3 d = p - lookAt;

		float lenSqr = lengthSqr(d);
		if (lenSqr > (maxDistance * maxDistance))
		{
			d *= (maxDistance / sqrtf(lenSqr));
		}

		p = d + lookAt;
		pCameraController->moveTo(p);
		pCameraController->lookAt(lookAt);
	}

	void GenerateQuads()
	{
		// Vertex Buffer
		struct Vertex {
			float pos[3];
			float uv[2];
		};

		eastl::vector<Vertex> vertexBuffer;
		float x = 0.0f;
		float y = 0.0f;
		for (uint32_t i = 0; i < 3; i++)
		{
			// Last component of uv is used for debug display sampler index
			vertexBuffer.push_back({ { x + 1.0f,		y + 1.0f,		(float)i	}, { 1.0f, 1.0f } });
			vertexBuffer.push_back({ { x - 1.0f,		y + 1.0f,		(float)i	}, { 0.0f, 1.0f} });
			vertexBuffer.push_back({ { x - 1.0f,		y - 1.0f,		(float)i	}, { 0.0f, 0.0f} });
			vertexBuffer.push_back({ { x + 1.0f,		y - 1.0f,		(float)i	}, { 1.0f, 0.0f } });
			x += 1.0f;
			if (x > 1.0f)
			{
				x = 0.0f;
				y += 1.0f;
			}
		}
		quads.vertexCount = vertexBuffer.size();

		BufferLoadDesc quadVBufferDesc = {};
		quadVBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		quadVBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		quadVBufferDesc.mDesc.mSize = sizeof(Vertex) * quads.vertexCount;
		quadVBufferDesc.mDesc.mVertexStride = sizeof(Vertex);
		quadVBufferDesc.pData = vertexBuffer.data();
		quadVBufferDesc.ppBuffer = &quads.vertexBuffer;
		addResource(&quadVBufferDesc);

		// Index Buffer
		eastl::vector<uint32_t> indexBuffer = eastl::vector<uint32_t>(6);
		indexBuffer[0] = 0;
		indexBuffer[1] = 1;
		indexBuffer[2] = 2;
		indexBuffer[3] = 2;
		indexBuffer[4] = 3;
		indexBuffer[5] = 0;

		for (uint32_t i = 0; i < 3; ++i)
		{
			uint32_t indices[6] = { 0,1,2, 2,3,0 };
			for (auto index : indices)
			{
				indexBuffer.push_back(i * 4 + index);
			}
		}

		if (indexBuffer[4] == 3)
		{
			quads.indexCount = static_cast<uint32_t>(indexBuffer.size());
		}

		BufferLoadDesc quadIndexBufferDesc = {};
		quadIndexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		quadIndexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		quadIndexBufferDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		quadIndexBufferDesc.mDesc.mSize = sizeof(uint32_t) * quads.indexCount;
		quadIndexBufferDesc.pData = indexBuffer.data();
		quadIndexBufferDesc.ppBuffer = &quads.indexBuffer;
		addResource(&quadIndexBufferDesc);
	}

	void CreateLightsBuffer()
	{
		BufferLoadDesc bufferDesc = {};

		// Light Uniform Buffer
		bufferDesc = {};
		bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufferDesc.mDesc.mSize = sizeof(lightData);
		bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufferDesc.pData = NULL;
		bufferDesc.ppBuffer = &pLightBuffer;
		addResource(&bufferDesc);

		// DirLights Structured Buffer
		bufferDesc = {};
		bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		bufferDesc.mDesc.mFirstElement = 0;
		bufferDesc.mDesc.mElementCount = gDirectionalLights;
		bufferDesc.mDesc.mStructStride = sizeof(DirectionalLight);
		bufferDesc.mDesc.mSize = bufferDesc.mDesc.mStructStride * bufferDesc.mDesc.mElementCount;
		bufferDesc.pData = NULL;
		bufferDesc.ppBuffer = &lightBuffers.pDirLightsBuffer;
		addResource(&bufferDesc);

		// PointLights Structured Buffer
		bufferDesc = {};
		bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		bufferDesc.mDesc.mFirstElement = 0;
		bufferDesc.mDesc.mElementCount = gPointLights;
		bufferDesc.mDesc.mStructStride = sizeof(PointLight);
		bufferDesc.mDesc.mSize = bufferDesc.mDesc.mStructStride * bufferDesc.mDesc.mElementCount;
		bufferDesc.pData = NULL;
		bufferDesc.ppBuffer = &lightBuffers.pPointLightsBuffer;
		addResource(&bufferDesc);

		// SpotLights Structured Buffer
		bufferDesc = {};
		bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		bufferDesc.mDesc.mFirstElement = 0;
		bufferDesc.mDesc.mElementCount = gSpotLights;
		bufferDesc.mDesc.mStructStride = sizeof(SpotLight);
		bufferDesc.mDesc.mSize = bufferDesc.mDesc.mStructStride * bufferDesc.mDesc.mElementCount;
		bufferDesc.pData = NULL;
		bufferDesc.ppBuffer = &lightBuffers.pSpotLightsBuffer;
		addResource(&bufferDesc);
	}

	bool LoadModels()
	{
		// Main Texture
		TextureLoadDesc textureDesc = {};
		textureDesc.mRoot = FSR_Textures;
		textureDesc.pFilename = pTexturesFileNames[0];
		textureDesc.ppTexture = &textures.pTextureColor;
		addResource(&textureDesc, true);

		// Specular Texture
		textureDesc.pFilename = pTexturesFileNames[1];
		textureDesc.ppTexture = &textures.pTextureSpecular;
		addResource(&textureDesc, true);

		AssimpImporter importer;

		AssimpImporter::Model model;
		if (!importer.ImportModel("../../../../art/Meshes/lion.obj", &model))
		{
			return false;
		}

		size_t meshSize = model.mMeshArray.size();

		for (size_t i = 0; i < meshSize; ++i)
		{
			AssimpImporter::Mesh subMesh = model.mMeshArray[i];

			MeshBatch* pMeshBatch = (MeshBatch*)conf_placement_new<MeshBatch>(conf_calloc(1, sizeof(MeshBatch)));

			sceneData.meshes.push_back(pMeshBatch);

			// Vertex Buffer
			BufferLoadDesc bufferDesc = {};
			bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
			bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			bufferDesc.mDesc.mVertexStride = sizeof(float3);
			bufferDesc.mDesc.mSize = bufferDesc.mDesc.mVertexStride * subMesh.mPositions.size();
			bufferDesc.pData = subMesh.mPositions.data();
			bufferDesc.ppBuffer = &pMeshBatch->pPositionStream;
			addResource(&bufferDesc);

			bufferDesc.mDesc.mVertexStride = sizeof(float3);
			bufferDesc.mDesc.mSize = subMesh.mNormals.size() * bufferDesc.mDesc.mVertexStride;
			bufferDesc.pData = subMesh.mNormals.data();
			bufferDesc.ppBuffer = &pMeshBatch->pNormalStream;
			addResource(&bufferDesc);

			bufferDesc.mDesc.mVertexStride = sizeof(float2);
			bufferDesc.mDesc.mSize = subMesh.mUvs.size() * bufferDesc.mDesc.mVertexStride;
			bufferDesc.pData = subMesh.mUvs.data();
			bufferDesc.ppBuffer = &pMeshBatch->pUVStream;
			addResource(&bufferDesc);

			pMeshBatch->mCountIndices = subMesh.mIndices.size();

			// Index buffer
			bufferDesc = {};
			bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
			bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			bufferDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
			bufferDesc.mDesc.mSize = sizeof(uint) * (uint)subMesh.mIndices.size();
			bufferDesc.pData = subMesh.mIndices.data();
			bufferDesc.ppBuffer = &pMeshBatch->pIndicesStream;
			addResource(&bufferDesc);
		}

		return true;
	}
};

DEFINE_APPLICATION_MAIN(DefferedLighting)