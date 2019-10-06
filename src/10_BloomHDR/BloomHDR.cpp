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

constexpr size_t gPointLights = 2;
constexpr size_t gMaxPointLights = 8;
static_assert(gPointLights <= gMaxPointLights, "");

constexpr size_t gSpotLights = 1;
constexpr size_t gMaxSpotLights = 8;
static_assert(gSpotLights <= gMaxSpotLights, "");

// should be in sync with shader definitions
constexpr size_t bloom_dim = 256;

const uint32_t	gImageCount = 3;

Renderer* pRenderer = NULL;

Queue* pGraphicsQueue = NULL;

Fence* pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };
Semaphore* pImageAquiredSemaphore = NULL;
Sampler* pSampler;

SwapChain* pSwapChain = NULL;

uint32_t			gFrameIndex = 0;

bool           gMicroProfiler = false;
bool           bPrevToggleMicroProfiler = false;

UIApp gAppUI;
GuiComponent* pGui = NULL;
VirtualJoystickUI gVirtualJoystick;
GpuProfiler* pGpuProfiler = NULL;
ICameraController* pCameraController = NULL;
TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

RenderTarget* pDepthBuffer;
RenderTarget* pBloomDepthStencilBuffer;

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


struct
{
	RenderTarget* hdr[gImageCount];
	RenderTarget* bloom[gImageCount];
	RenderTarget* blurred[gImageCount];
} renderTargets;

// TODO: Find Good Data Representation of RenderPasses and Pipelines and ...
class RenderPassData
{
public:
	Pipeline* pPipeline;
	Shader* pShader;
	RootSignature* pRootSignature;
	DescriptorSet* pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
	CmdPool* pCmdPool;
	Cmd** ppCmds;

	RenderPassData()
	{
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);
	}
};

struct RenderPass
{
	enum Enum
	{
		Forward,
		Light,
		BloomDownres,
		Blur,
		ToneMapping,
		RENDER_PASS_COUNT
	};
};

typedef eastl::unordered_map<RenderPass::Enum, RenderPassData*> RenderPassMap;

RenderPassMap	RenderPasses;

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
	eastl::vector<MeshBatch*> scene;
	MeshBatch* sphere;
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

Buffer* pLightBuffer = NULL;

struct
{
	mat4	view;
	mat4	proj;
	mat4	pToWorld;
} gUniformData;

struct
{
	float3 position[gMaxPointLights];
	float3 color[gMaxPointLights];
} lightInstancedata;

Buffer* pInstancePositionBuffer;
Buffer* pInstanceColorBuffer;

Buffer* pUniformBuffers[gImageCount] = { NULL };

struct
{
	float exposure = 1.0f;
	float gamma = 2.2f;
	float bloomLevel = 1.0f;
	bool tonemap = false;
} toneMappingData;

Buffer* pToneMappingBuffer[gImageCount];

const char* pszBases[FSR_Count] = {
	"../../../../src/Shaders/bin",													// FSR_BinShaders
	"../../../../src/10_BloomHDR/",												// FSR_SrcShaders
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

bool gPauseLights = false;

class BloomHDR : public IApp
{
public:
	BloomHDR()
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

		for (int i = 0; i < RenderPass::RENDER_PASS_COUNT; ++i)
		{
			RenderPassData* pass = conf_placement_new<RenderPassData>(conf_calloc(1, sizeof(RenderPassData)));
			RenderPasses.insert(eastl::pair<RenderPass::Enum, RenderPassData*>((RenderPass::Enum)(i), pass));
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAquiredSemaphore);

		// Resource Loading
		initResourceLoaderInterface(pRenderer);

		// Shader
		ShaderLoadDesc shaderDesc = {};
		shaderDesc.mStages[0] = { "basic.vert", NULL, 0, FSR_SrcShaders };
		shaderDesc.mStages[1] = { "basic.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &shaderDesc, &RenderPasses[RenderPass::Forward]->pShader);

		shaderDesc.mStages[0] = { "light.vert", NULL, 0, FSR_SrcShaders };
		shaderDesc.mStages[1] = { "light.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &shaderDesc, &RenderPasses[RenderPass::Light]->pShader);

		shaderDesc.mStages[0] = { "bloom/bloom.vert", NULL, 0, FSR_SrcShaders };
		shaderDesc.mStages[1] = { "bloom/bloom.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &shaderDesc, &RenderPasses[RenderPass::BloomDownres]->pShader);

		shaderDesc.mStages[0] = { "bloom/gaussianBlurV.vert", NULL, 0, FSR_SrcShaders };
		shaderDesc.mStages[1] = { "bloom/gaussianBlurV.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &shaderDesc, &RenderPasses[RenderPass::Blur]->pShader);

		shaderDesc.mStages[0] = { "tonemapping.vert", NULL, 0, FSR_SrcShaders };
		shaderDesc.mStages[1] = { "tonemapping.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &shaderDesc, &RenderPasses[RenderPass::ToneMapping]->pShader);

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
									MIPMAP_MODE_LINEAR,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									0,
									0 };
		addSampler(pRenderer, &samplerDesc, &pSampler);

		// Resource Binding
		const char* samplerNames = { "uSampler0 " };
		{
			// RenderPass::Forward
			{
				Shader* shaders[2] = { RenderPasses[RenderPass::Forward]->pShader, RenderPasses[RenderPass::BloomDownres]->pShader };
				RootSignatureDesc rootDesc = {};
				rootDesc.mShaderCount = 1;
				rootDesc.ppShaders = shaders;
				rootDesc.mStaticSamplerCount = 1;
				rootDesc.ppStaticSamplerNames = &samplerNames;
				rootDesc.ppStaticSamplers = &pSampler;
				addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::Forward]->pRootSignature);

				DescriptorSetDesc setDesc = { RenderPasses[RenderPass::Forward]->pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
				addDescriptorSet(pRenderer, &setDesc, &RenderPasses[RenderPass::Forward]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_NONE]);
				setDesc = { RenderPasses[RenderPass::Forward]->pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
				addDescriptorSet(pRenderer, &setDesc, &RenderPasses[RenderPass::Forward]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
			}

			// RenderPass::Light
			{
				RootSignatureDesc rootDesc = {};
				rootDesc.mShaderCount = 1;
				rootDesc.ppShaders = &RenderPasses[RenderPass::Light]->pShader;
				addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::Light]->pRootSignature);

				DescriptorSetDesc setDesc = { RenderPasses[RenderPass::Light]->pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
				addDescriptorSet(pRenderer, &setDesc, &RenderPasses[RenderPass::Light]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_NONE]);
				setDesc = { RenderPasses[RenderPass::Light]->pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
				addDescriptorSet(pRenderer, &setDesc, &RenderPasses[RenderPass::Light]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
			}

			// RenderPass::Blur
			{
				RootSignatureDesc rootDesc = {};
				rootDesc.mShaderCount = 1;
				rootDesc.ppShaders = &RenderPasses[RenderPass::Blur]->pShader;
				rootDesc.mStaticSamplerCount = 1;
				rootDesc.ppStaticSamplerNames = &samplerNames;
				rootDesc.ppStaticSamplers = &pSampler;
				addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::Blur]->pRootSignature);

				DescriptorSetDesc setDesc = { RenderPasses[RenderPass::Blur]->pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
				addDescriptorSet(pRenderer, &setDesc, &RenderPasses[RenderPass::Blur]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_NONE]);
				setDesc = { RenderPasses[RenderPass::Blur]->pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
				addDescriptorSet(pRenderer, &setDesc, &RenderPasses[RenderPass::Blur]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
			}

			// RenderPass::ToneMapping
			{
				RootSignatureDesc rootDesc = {};
				rootDesc.mShaderCount = 1;
				rootDesc.ppShaders = &RenderPasses[RenderPass::ToneMapping]->pShader;
				rootDesc.mStaticSamplerCount = 1;
				rootDesc.ppStaticSamplerNames = &samplerNames;
				rootDesc.ppStaticSamplers = &pSampler;
				addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::ToneMapping]->pRootSignature);

				DescriptorSetDesc setDesc = { RenderPasses[RenderPass::ToneMapping]->pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
				addDescriptorSet(pRenderer, &setDesc, &RenderPasses[RenderPass::ToneMapping]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_NONE]);
				setDesc = { RenderPasses[RenderPass::ToneMapping]->pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
				addDescriptorSet(pRenderer, &setDesc, &RenderPasses[RenderPass::ToneMapping]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
			}
		}

		// Create Uniform Buffers
		{
			// Forward
			{
				BufferLoadDesc bufferDesc = {};
				bufferDesc = {};
				bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				bufferDesc.mDesc.mSize = sizeof(gUniformData);
				bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				bufferDesc.pData = NULL;

				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					bufferDesc.ppBuffer = &pUniformBuffers[i];
					addResource(&bufferDesc);
				}
			}

			// Instance
			{
				BufferLoadDesc bufferDesc = {};
				bufferDesc = {};
				bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
				bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				bufferDesc.mDesc.mVertexStride = sizeof(float3);
				bufferDesc.mDesc.mSize = sizeof(float3) * gMaxPointLights;
				bufferDesc.mDesc.mStartState = RESOURCE_STATE_GENERIC_READ;
				bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				bufferDesc.pData = NULL;
				bufferDesc.ppBuffer = &pInstancePositionBuffer;
				addResource(&bufferDesc);
				bufferDesc.ppBuffer = &pInstanceColorBuffer;
				addResource(&bufferDesc);
			}

			// Tone Mapping
			{
				BufferLoadDesc bufferDesc = {};
				bufferDesc = {};
				bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				bufferDesc.mDesc.mSize = sizeof(toneMappingData);
				bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				bufferDesc.pData = NULL;

				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					bufferDesc.ppBuffer = &pToneMappingBuffer[i];
					addResource(&bufferDesc);
				}
			}
		}

		CreateLightsBuffer();

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
		pGui->AddWidget(CheckboxWidget("ToneMapping", &toneMappingData.tonemap));
		pGui->AddWidget(SliderFloatWidget("Exposure", &toneMappingData.exposure, 0.1f, 10.0f, 0.25f, "%.2f"));
		pGui->AddWidget(SliderFloatWidget("Gamma", &toneMappingData.gamma, 1.0f, 3.0f, 0.6f, "%.2f"));
		pGui->AddWidget(SliderFloatWidget("Bloom Level", &toneMappingData.bloomLevel, 0.0f, 1.0f, 0.05f, "%.2f"));
		pGui->AddWidget(CheckboxWidget("Pause Lights", &gPauseLights));

		// Camera
		CameraMotionParameters cmp{ 40.0f, 30.0f, 100.0f };
		vec3                   camPos{ 0.0f, 0.0f, 0.8f };
		vec3                   lookAt{ 0.0f, 0.0f, 5.0f };

		pCameraController = createFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);
		if (!initInputSystem(pWindow))
			return false;

		// Initialize microprofiler and it's UI.
		initProfiler();

		// Gpu profiler can only be added after initProfile.
		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

		// App Actions
		InputActionDesc actionDesc = { InputBindings::BUTTON_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
		addInputAction(&actionDesc);
		actionDesc =
		{
			InputBindings::BUTTON_ANY, [](InputActionContext* ctx)
			{
				bool capture = gAppUI.OnButton(ctx->mBinding, ctx->mBool, ctx->pPosition);
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
			removeResource(pUniformBuffers[i]);
			removeResource(pToneMappingBuffer[i]);
		}

		removeResource(pInstancePositionBuffer);
		removeResource(pInstanceColorBuffer);

		removeResource(lightBuffers.pDirLightsBuffer);
		removeResource(lightBuffers.pPointLightsBuffer);
		removeResource(lightBuffers.pSpotLightsBuffer);

		//Delete rendering passes
		for (RenderPassMap::iterator iter = RenderPasses.begin(); iter != RenderPasses.end(); ++iter)
		{
			RenderPassData* pass = iter->second;

			if (!pass)
				continue;

			for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
				if (pass->pDescriptorSets[i])
					removeDescriptorSet(pRenderer, pass->pDescriptorSets[i]);

			removeShader(pRenderer, pass->pShader);

			if (pass->pRootSignature)
				removeRootSignature(pRenderer, pass->pRootSignature);

			removeCmd_n(pass->pCmdPool, gImageCount, pass->ppCmds);
			removeCmdPool(pRenderer, pass->pCmdPool);

			pass->~RenderPassData();
			conf_free(pass);
		}

		RenderPasses.empty();

		for (size_t i = 0; i < sceneData.scene.size(); ++i)
		{
			removeMesh(sceneData.scene[i]);
		}
		removeMesh(sceneData.sphere);

		removeResource(textures.pTextureColor);
		removeResource(textures.pTextureSpecular);

		removeRasterizerState(pRasterDefault);
		removeDepthState(pDepthDefault);
		removeDepthState(pDepthNone);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAquiredSemaphore);

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

		loadProfiler(&gAppUI, mSettings.mWidth, mSettings.mHeight);

		CreatePipelines();

		PrepareDescriptorSets();

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

			if (pass->pPipeline)
			{
				removePipeline(pRenderer, pass->pPipeline);
				pass->pPipeline = NULL;
			}
		}

		for (int i = 0; i < gImageCount; ++i)
			removeRenderTarget(pRenderer, renderTargets.hdr[i]);
		for (int i = 0; i < gImageCount; ++i)
			removeRenderTarget(pRenderer, renderTargets.bloom[i]);
		for (int i = 0; i < gImageCount; ++i)
			removeRenderTarget(pRenderer, renderTargets.blurred[i]);

		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pBloomDepthStencilBuffer);
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

		static float currentLightTime;
		if (!gPauseLights)
			currentLightTime = 100;

		// update camera with time
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.01f, 500);

		gUniformData.view = viewMat;
		gUniformData.proj = projMat;

		// Update Instance Data
		gUniformData.pToWorld = mat4::translation(Vector3(0.0f, -1, 6)) *
			//mat4::rotationY(currentTime) *
			mat4::scale(Vector3(0.1f));

		viewMat.setTranslation(vec3(0));

		directionalLights[0].direction = float3{ 0.0f, -1.0f, 0.0f };
		directionalLights[0].ambient = float3{ 0.05f, 0.05f, 0.05f };
		directionalLights[0].diffuse = float3{ 0.5f, 0.5f, 0.5f };
		directionalLights[0].specular = float3{ 0.5f, 0.5f, 0.5f };
		lightData.numDirectionalLights = 0;

		pointLights[0].position = float3{ 0.0f + 4 * abs(cos(2 * currentLightTime)), 1.0f, 4.0f };
		pointLights[0].ambient = float3{ 0.02f, 0.02f, 0.02f };
		pointLights[0].diffuse = float3{ 1.0f, 2.0f, 2.0f };
		pointLights[0].specular = float3{ 1.0f, 3.0f, 3.0f };
		pointLights[0].attenuationParams = float3{ 1.0f, 0.07f, 0.017f };

		pointLights[1].position = float3{ 0.0f - 4 * abs(cos(2 * currentLightTime)), 1.0f, 4.0f };
		pointLights[1].ambient = float3{ 0.02f, 0.02f, 0.02f };
		pointLights[1].diffuse = float3{ 1.0f, 0.0f, 2.0f };
		pointLights[1].specular = float3{ 1.0f, 0.0f, 2.0f };
		pointLights[1].attenuationParams = float3{ 1.0f, 0.07f, 0.017f };
		lightData.numPointLights = gPointLights;

		for (int i = 0; i < gPointLights; ++i)
		{
			lightInstancedata.position[i] = pointLights[i].position;
			lightInstancedata.color[i] = pointLights[i].diffuse;
		}

		spotLights[0].position = float3{ 4.0f * sin(currentTime), 0.0f, 0.0f };
		spotLights[0].direction = float3{ -0.5f, 0.0f, 1.0f };
		spotLights[0].cutOffs = float2{ cos(PI / 9), cos(PI / 6) };
		spotLights[0].ambient = float3{ 0.1f, 0.1f, 0.1f };
		spotLights[0].diffuse = float3{ 1.0f, 1.0f, 1.0f };
		spotLights[0].specular = float3{ 1.0f, 1.0f, 1.0f };
		spotLights[0].attenuationParams = float3{ 1.0f, 0.07f, 0.017f };
		lightData.numSpotLights = 0;


		lightData.viewPos = v3ToF3(pCameraController->getViewPosition());

		if (gMicroProfiler != bPrevToggleMicroProfiler)
		{
			toggleProfiler();
			bPrevToggleMicroProfiler = gMicroProfiler;
		}

		/************************************************************************/
		// Update GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		Cmd* allCmds[4];
		acquireNextImage(pRenderer, pSwapChain, pImageAquiredSemaphore, NULL, &gFrameIndex);

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		// Update uniform buffers
		BufferUpdateDesc viewProjCbv = { pUniformBuffers[gFrameIndex], &gUniformData };
		updateResource(&viewProjCbv);

		// Update tone mapping uniform buffers
		BufferUpdateDesc toneBuffUpdate = { pToneMappingBuffer[gFrameIndex], &toneMappingData };
		updateResource(&toneBuffUpdate);

		// Update light uniform buffers
		BufferUpdateDesc lightBuffUpdate = { pLightBuffer, &lightData };
		updateResource(&lightBuffUpdate);

		BufferUpdateDesc pointLightBuffUpdate = { lightBuffers.pPointLightsBuffer, &pointLights };
		updateResource(&pointLightBuffUpdate);

		BufferUpdateDesc dirLightBuffUpdate = { lightBuffers.pDirLightsBuffer, &directionalLights };
		updateResource(&dirLightBuffUpdate);

		BufferUpdateDesc spotLightBuffUpdate = { lightBuffers.pSpotLightsBuffer, &spotLights };
		updateResource(&spotLightBuffUpdate);

		BufferUpdateDesc instanceDataUpdate = { pInstancePositionBuffer, &lightInstancedata.position };
		updateResource(&instanceDataUpdate);
		instanceDataUpdate = { pInstanceColorBuffer, &lightInstancedata.color };
		updateResource(&instanceDataUpdate);

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

		RenderTarget* pSwapChainRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		RenderTarget* pHdrRenderTarget = renderTargets.hdr[gFrameIndex];
		RenderTarget* pBloomRenderTarget = renderTargets.bloom[gFrameIndex];
		RenderTarget* pBlurRenderTarget = renderTargets.blurred[gFrameIndex];

		Cmd* cmd = RenderPasses[RenderPass::Forward]->ppCmds[gFrameIndex];
		beginCmd(cmd);
		{
			cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Render Scene Pass", true);
			{
				TextureBarrier textureBarriers[2] = {
					{ pHdrRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
					{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE }
				};

				cmdResourceBarrier(cmd, 0, nullptr, 2, textureBarriers);

				loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
				loadActions.mClearColorValues[0] = pHdrRenderTarget->mDesc.mClearValue;

				cmdBindRenderTargets(
					cmd,
					1,
					&pHdrRenderTarget,
					pDepthBuffer,
					&loadActions, NULL, NULL, -1, -1);

				cmdSetViewport(
					cmd, 0.0f, 0.0f, (float)pHdrRenderTarget->mDesc.mWidth,
					(float)pHdrRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
				cmdSetScissor(
					cmd, 0, 0, pHdrRenderTarget->mDesc.mWidth,
					pHdrRenderTarget->mDesc.mHeight);

				cmdBindPipeline(cmd, RenderPasses[RenderPass::Forward]->pPipeline);
				{
					cmdBindDescriptorSet(cmd, 0, RenderPasses[RenderPass::Forward]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_NONE]);
					cmdBindDescriptorSet(cmd, gFrameIndex, RenderPasses[RenderPass::Forward]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);

					for (int i = 0; i < sceneData.scene.size(); ++i)
					{
						Buffer* pVertexBuffers[] = { sceneData.scene[i]->pPositionStream, sceneData.scene[i]->pNormalStream, sceneData.scene[i]->pUVStream };
						cmdBindVertexBuffer(cmd, 3, pVertexBuffers, NULL);
						cmdBindIndexBuffer(cmd, sceneData.scene[i]->pIndicesStream, 0);
						cmdDrawIndexed(cmd, sceneData.scene[0]->mCountIndices, 0, 0);
					}
				}

				cmdBindPipeline(cmd, RenderPasses[RenderPass::Light]->pPipeline);
				{
					cmdBindDescriptorSet(cmd, 0, RenderPasses[RenderPass::Light]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_NONE]);
					cmdBindDescriptorSet(cmd, gFrameIndex, RenderPasses[RenderPass::Light]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);

					Buffer* pVertexBuffers[] = { sceneData.sphere->pPositionStream, pInstancePositionBuffer, pInstanceColorBuffer };
					cmdBindVertexBuffer(cmd, 3, pVertexBuffers, NULL);
					cmdBindIndexBuffer(cmd, sceneData.sphere->pIndicesStream, 0);
					cmdDrawIndexedInstanced(cmd, sceneData.sphere->mCountIndices, 0, gPointLights, 0, 0);
				}
			}
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}
		endCmd(cmd);
		allCmds[0] = cmd;

		cmd = RenderPasses[RenderPass::BloomDownres]->ppCmds[gFrameIndex];
		beginCmd(cmd);
		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Bloom Pass", true);
			{
				TextureBarrier textureBarriers[2] = {
					{ pBloomRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
					{ pBloomDepthStencilBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE }
				};
				cmdResourceBarrier(cmd, 0, nullptr, 2, textureBarriers);

				loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
				loadActions.mClearDepth.depth = 1.0f;
				loadActions.mLoadActionStencil = LOAD_ACTION_CLEAR;
				loadActions.mClearDepth.stencil = 0;
				loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
				loadActions.mClearColorValues[0] = pBloomRenderTarget->mDesc.mClearValue;

				cmdBindRenderTargets(
					cmd,
					1,
					&pBloomRenderTarget,
					pBloomDepthStencilBuffer,
					&loadActions, NULL, NULL, -1, -1);

				cmdSetViewport(
					cmd, 0.0f, 0.0f, (float)pBloomRenderTarget->mDesc.mWidth,
					(float)pBloomRenderTarget->mDesc.mHeight, 0.0f, 1.0f);

				cmdSetScissor(
					cmd, 0, 0, pBloomRenderTarget->mDesc.mWidth,
					pBloomRenderTarget->mDesc.mHeight);

				cmdBindPipeline(cmd, RenderPasses[RenderPass::BloomDownres]->pPipeline);
				{
					cmdBindDescriptorSet(cmd, 0, RenderPasses[RenderPass::Forward]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_NONE]);
					cmdBindDescriptorSet(cmd, gFrameIndex, RenderPasses[RenderPass::Forward]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);

					for (int i = 0; i < sceneData.scene.size(); ++i)
					{
						Buffer* pVertexBuffers[] = { sceneData.scene[i]->pPositionStream, sceneData.scene[i]->pNormalStream, sceneData.scene[i]->pUVStream };
						cmdBindVertexBuffer(cmd, 3, pVertexBuffers, NULL);
						cmdBindIndexBuffer(cmd, sceneData.scene[i]->pIndicesStream, 0);
						cmdDrawIndexed(cmd, sceneData.scene[0]->mCountIndices, 0, 0);
					}
				}
			}
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}
		endCmd(cmd);
		allCmds[1] = cmd;

		cmd = RenderPasses[RenderPass::Blur]->ppCmds[gFrameIndex];
		beginCmd(cmd);
		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Blur Pass", true);
			{
				TextureBarrier textureBarriers[2] = {
					{ pBlurRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
					{ pBloomRenderTarget->pTexture, RESOURCE_STATE_SHADER_RESOURCE }
				};
				cmdResourceBarrier(cmd, 0, nullptr, 2, textureBarriers);

				loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
				loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;
				cmdBindRenderTargets(
					cmd,
					1,
					&pBlurRenderTarget,
					NULL,
					&loadActions, NULL, NULL, -1, -1);

				cmdSetViewport(
					cmd, 0.0f, 0.0f, (float)pBlurRenderTarget->mDesc.mWidth,
					(float)pBlurRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
				cmdSetScissor(
					cmd, 0, 0, pBlurRenderTarget->mDesc.mWidth,
					pBlurRenderTarget->mDesc.mHeight);

				cmdBindPipeline(cmd, RenderPasses[RenderPass::Blur]->pPipeline);
				{
					cmdBindDescriptorSet(cmd, 0, RenderPasses[RenderPass::Blur]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_NONE]);
					cmdBindDescriptorSet(cmd, gFrameIndex, RenderPasses[RenderPass::Blur]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
					cmdDraw(cmd, 3, 0);
				}
			}
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}
		endCmd(cmd);
		allCmds[2] = cmd;

		cmd = RenderPasses[RenderPass::ToneMapping]->ppCmds[gFrameIndex];
		beginCmd(cmd);
		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Composition and ToneMapping Pass", true);
			{
				TextureBarrier textureBarriers[3] = {
					{ pHdrRenderTarget->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
					{ pBlurRenderTarget->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
					{ pSwapChainRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET }
				};
				cmdResourceBarrier(cmd, 0, nullptr, 3, textureBarriers);

				loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
				loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;
				cmdBindRenderTargets(
					cmd,
					1,
					&pSwapChainRenderTarget,
					NULL,
					&loadActions, NULL, NULL, -1, -1);

				cmdSetViewport(
					cmd, 0.0f, 0.0f, (float)pSwapChainRenderTarget->mDesc.mWidth,
					(float)pSwapChainRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
				cmdSetScissor(
					cmd, 0, 0, pSwapChainRenderTarget->mDesc.mWidth,
					pSwapChainRenderTarget->mDesc.mHeight);

				cmdBindPipeline(cmd, RenderPasses[RenderPass::ToneMapping]->pPipeline);
				{
					cmdBindDescriptorSet(cmd, 0, RenderPasses[RenderPass::ToneMapping]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_NONE]);
					cmdBindDescriptorSet(cmd, gFrameIndex, RenderPasses[RenderPass::ToneMapping]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
					cmdDraw(cmd, 3, 0);
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

				cmdDrawProfiler();

				gAppUI.Draw(cmd);
				cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
			}
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

			TextureBarrier textureBarrier = { pSwapChainRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
			cmdResourceBarrier(cmd, 0, NULL, 1, &textureBarrier);
			cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		}
		endCmd(cmd);
		allCmds[3] = cmd;

		queueSubmit(pGraphicsQueue, 4, allCmds, pRenderCompleteFence, 1, &pImageAquiredSemaphore, 1, &pRenderCompleteSemaphore);

		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);

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

		// HDR
		{
			RenderTargetDesc rtDesc = {};
			rtDesc.mClearValue = colorClearBlack;
			rtDesc.mArraySize = 1;
			rtDesc.mDepth = 1;
			rtDesc.mWidth = mSettings.mWidth;
			rtDesc.mHeight = mSettings.mHeight;
			rtDesc.mSampleCount = SAMPLE_COUNT_1;
			rtDesc.mSampleQuality = 0;
			rtDesc.pDebugName = L"HDR";

			rtDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
			for (int i = 0; i < gImageCount; ++i)
				addRenderTarget(pRenderer, &rtDesc, &renderTargets.hdr[i]);
		}

		// Bright
		{
			RenderTargetDesc rtDesc = {};
			rtDesc.mClearValue = colorClearBlack;
			rtDesc.mArraySize = 1;
			rtDesc.mDepth = 1;
			rtDesc.mWidth = bloom_dim;
			rtDesc.mHeight = bloom_dim;
			rtDesc.mSampleCount = SAMPLE_COUNT_1;
			rtDesc.mSampleQuality = 0;
			rtDesc.pDebugName = L"Bright";

			rtDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
			for (int i = 0; i < gImageCount; ++i)
				addRenderTarget(pRenderer, &rtDesc, &renderTargets.bloom[i]);
		}

		// Blur
		{
			RenderTargetDesc rtDesc = {};
			rtDesc.mClearValue = colorClearBlack;
			rtDesc.mArraySize = 1;
			rtDesc.mDepth = 1;
			rtDesc.mWidth = bloom_dim;
			rtDesc.mHeight = bloom_dim;
			rtDesc.mSampleCount = SAMPLE_COUNT_1;
			rtDesc.mSampleQuality = 0;
			rtDesc.pDebugName = L"Blurr";

			rtDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
			for (int i = 0; i < gImageCount; ++i)
				addRenderTarget(pRenderer, &rtDesc, &renderTargets.blurred[i]);
		}

		// Depth Buffer
		{
			RenderTargetDesc rtDesc = {};
			rtDesc.mArraySize = 1;
			rtDesc.mClearValue.depth = 1.0f;
			rtDesc.mClearValue.stencil = 0.0f;
			rtDesc.mFormat = TinyImageFormat_D32_SFLOAT;
			rtDesc.mDepth = 1;
			rtDesc.mWidth = mSettings.mWidth;
			rtDesc.mHeight = mSettings.mHeight;
			rtDesc.mSampleCount = SAMPLE_COUNT_1;
			rtDesc.mSampleQuality = 0;
			addRenderTarget(pRenderer, &rtDesc, &pDepthBuffer);
		}

		// Bloom DepthStencil Buffer
		{
			RenderTargetDesc rtDesc = {};
			rtDesc.mArraySize = 1;
			rtDesc.mClearValue.depth = 1.0f;
			rtDesc.mClearValue.stencil = 0.0f;
			rtDesc.mFormat = TinyImageFormat_D24_UNORM_S8_UINT;
			rtDesc.mDepth = 1;
			rtDesc.mWidth = bloom_dim;
			rtDesc.mHeight = bloom_dim;
			rtDesc.mSampleCount = SAMPLE_COUNT_1;
			rtDesc.mSampleQuality = 0;
			addRenderTarget(pRenderer, &rtDesc, &pBloomDepthStencilBuffer);
		}
	}

	void CreatePipelines()
	{
		// Forward
		{
			VertexLayout vertexLayout = {};
			vertexLayout.mAttribCount = 3;

			vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
			vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
			vertexLayout.mAttribs[0].mBinding = 0;
			vertexLayout.mAttribs[0].mLocation = 0;
			vertexLayout.mAttribs[0].mOffset = 0;

			vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
			vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
			vertexLayout.mAttribs[1].mBinding = 1;
			vertexLayout.mAttribs[1].mLocation = 1;
			vertexLayout.mAttribs[1].mOffset = 0;

			vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
			vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
			vertexLayout.mAttribs[2].mBinding = 2;
			vertexLayout.mAttribs[2].mLocation = 2;
			vertexLayout.mAttribs[2].mOffset = 0;

			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = pDepthDefault;
			pipelineSettings.pColorFormats = &renderTargets.hdr[0]->mDesc.mFormat;
			pipelineSettings.mSampleCount = renderTargets.hdr[0]->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = renderTargets.hdr[0]->mDesc.mSampleQuality;
			pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
			pipelineSettings.pRootSignature = RenderPasses[RenderPass::Forward]->pRootSignature;
			pipelineSettings.pShaderProgram = RenderPasses[RenderPass::Forward]->pShader;
			pipelineSettings.pVertexLayout = &vertexLayout;
			pipelineSettings.pRasterizerState = pRasterDefault;

			addPipeline(pRenderer, &desc, &RenderPasses[RenderPass::Forward]->pPipeline);
		}

		// Light
		{
			VertexLayout vertexLayout = {};
			vertexLayout.mAttribCount = 3;

			vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
			vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
			vertexLayout.mAttribs[0].mBinding = 0;
			vertexLayout.mAttribs[0].mLocation = 0;
			vertexLayout.mAttribs[0].mOffset = 0;

			vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
			vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
			vertexLayout.mAttribs[1].mRate = VERTEX_ATTRIB_RATE_INSTANCE;
			vertexLayout.mAttribs[1].mBinding = 1;
			vertexLayout.mAttribs[1].mLocation = 1;
			vertexLayout.mAttribs[1].mOffset = 0;

			vertexLayout.mAttribs[2].mSemantic = SEMANTIC_COLOR;
			vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
			vertexLayout.mAttribs[2].mRate = VERTEX_ATTRIB_RATE_INSTANCE;
			vertexLayout.mAttribs[2].mBinding = 2;
			vertexLayout.mAttribs[2].mLocation = 2;
			vertexLayout.mAttribs[2].mOffset = 0;

			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = pDepthDefault;
			pipelineSettings.pColorFormats = &renderTargets.hdr[0]->mDesc.mFormat;
			pipelineSettings.mSampleCount = renderTargets.hdr[0]->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = renderTargets.hdr[0]->mDesc.mSampleQuality;
			pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
			pipelineSettings.pRootSignature = RenderPasses[RenderPass::Light]->pRootSignature;
			pipelineSettings.pShaderProgram = RenderPasses[RenderPass::Light]->pShader;
			pipelineSettings.pVertexLayout = &vertexLayout;
			pipelineSettings.pRasterizerState = pRasterDefault;

			addPipeline(pRenderer, &desc, &RenderPasses[RenderPass::Light]->pPipeline);
		}

		// Bloom Forward
		{
			VertexLayout vertexLayout = {};
			vertexLayout.mAttribCount = 3;

			vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
			vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
			vertexLayout.mAttribs[0].mBinding = 0;
			vertexLayout.mAttribs[0].mLocation = 0;
			vertexLayout.mAttribs[0].mOffset = 0;

			vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
			vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
			vertexLayout.mAttribs[1].mBinding = 1;
			vertexLayout.mAttribs[1].mLocation = 1;
			vertexLayout.mAttribs[1].mOffset = 0;

			vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
			vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
			vertexLayout.mAttribs[2].mBinding = 2;
			vertexLayout.mAttribs[2].mLocation = 2;
			vertexLayout.mAttribs[2].mOffset = 0;

			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = pDepthDefault;
			pipelineSettings.pColorFormats = &renderTargets.bloom[0]->mDesc.mFormat;
			pipelineSettings.mSampleCount = renderTargets.bloom[0]->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = renderTargets.bloom[0]->mDesc.mSampleQuality;
			pipelineSettings.mDepthStencilFormat = pBloomDepthStencilBuffer->mDesc.mFormat;
			pipelineSettings.pRootSignature = RenderPasses[RenderPass::Forward]->pRootSignature;
			pipelineSettings.pShaderProgram = RenderPasses[RenderPass::BloomDownres]->pShader;
			pipelineSettings.pVertexLayout = &vertexLayout;
			pipelineSettings.pRasterizerState = pRasterDefault;

			addPipeline(pRenderer, &desc, &RenderPasses[RenderPass::BloomDownres]->pPipeline);
		}

		// Blur
		{
			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = pDepthNone;
			pipelineSettings.pColorFormats = &renderTargets.blurred[0]->mDesc.mFormat;
			pipelineSettings.mSampleCount = renderTargets.blurred[0]->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = renderTargets.blurred[0]->mDesc.mSampleQuality;
			pipelineSettings.pRootSignature = RenderPasses[RenderPass::Blur]->pRootSignature;
			pipelineSettings.pShaderProgram = RenderPasses[RenderPass::Blur]->pShader;
			pipelineSettings.pVertexLayout = NULL;
			pipelineSettings.pRasterizerState = pRasterDefault;

			addPipeline(pRenderer, &desc, &RenderPasses[RenderPass::Blur]->pPipeline);
		}

		// ToneMapping
		{
			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = pDepthNone;
			pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
			pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
			pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
			pipelineSettings.pRootSignature = RenderPasses[RenderPass::ToneMapping]->pRootSignature;
			pipelineSettings.pShaderProgram = RenderPasses[RenderPass::ToneMapping]->pShader;
			pipelineSettings.pVertexLayout = NULL;
			pipelineSettings.pRasterizerState = pRasterDefault;

			addPipeline(pRenderer, &desc, &RenderPasses[RenderPass::ToneMapping]->pPipeline);
		}
	}

	const char* GetName() { return "10_BloomHDR"; }

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

	void PrepareDescriptorSets()
	{
		// RenderPass::Forward
		{
			// DESCRIPTOR_UPDATE_FREQ_NONE
			{
				DescriptorData params[2] = {};
				params[0].pName = "Texture";
				params[0].ppTextures = &textures.pTextureColor;
				params[1].pName = "TextureSpecular";
				params[1].ppTextures = &textures.pTextureSpecular;
				updateDescriptorSet(pRenderer, 0, RenderPasses[RenderPass::Forward]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_NONE], 2, params);
			}

			// DESCRIPTOR_UPDATE_FREQ_PER_FRAME
			{

				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					DescriptorData params[5] = {};
					params[0].pName = "UniformData";
					params[0].ppBuffers = &pUniformBuffers[i];
					params[1].pName = "LightData";
					params[1].ppBuffers = &pLightBuffer;
					params[2].pName = "DirectionalLights";
					params[2].ppBuffers = &lightBuffers.pDirLightsBuffer;
					params[3].pName = "PointLights";
					params[3].ppBuffers = &lightBuffers.pPointLightsBuffer;
					params[4].pName = "SpotLights";
					params[4].ppBuffers = &lightBuffers.pSpotLightsBuffer;
					updateDescriptorSet(pRenderer, i, RenderPasses[RenderPass::Forward]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME], 5, params);
				}
			}
		}

		// RenderPass::Light
		{
			// DESCRIPTOR_UPDATE_FREQ_PER_FRAME
			{
				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					DescriptorData params[1] = {};
					params[0].pName = "UniformData";
					params[0].ppBuffers = &pUniformBuffers[i];
					updateDescriptorSet(pRenderer, i, RenderPasses[RenderPass::Light]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME], 1, params);
				}
			}
		}

		// RenderPass::Blur
		{
			// DESCRIPTOR_UPDATE_FREQ_PER_FRAME
			{
				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					DescriptorData params[2] = {};
					params[0].pName = "Texture";
					params[0].ppTextures = &renderTargets.bloom[i]->pTexture;
					//params[2].pName = "ToneMappingData";
					//params[2].ppBuffers = &pToneMappingBuffer[i];
					updateDescriptorSet(pRenderer, i, RenderPasses[RenderPass::Blur]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME], 1, params);
				}
			}
		}

		// RenderPass::ToneMapping
		{
			// DESCRIPTOR_UPDATE_FREQ_PER_FRAME
			{
				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					DescriptorData params[3] = {};
					params[0].pName = "HdrTexture";
					params[0].ppTextures = &renderTargets.hdr[i]->pTexture;
					params[0].pName = "BloomTexture";
					params[0].ppTextures = &renderTargets.bloom[i]->pTexture;
					params[2].pName = "ToneMappingData";
					params[2].ppBuffers = &pToneMappingBuffer[i];
					updateDescriptorSet(pRenderer, i, RenderPasses[RenderPass::ToneMapping]->pDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME], 1, params);
				}
			}
		}
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

		AssimpImporter::Model lionModel;

		{
			if (!importer.ImportModel("../../../../art/Meshes/lion.obj", &lionModel))
			{
				return false;
			}

			size_t meshSize = lionModel.mMeshArray.size();

			for (size_t i = 0; i < meshSize; ++i)
			{
				AssimpImporter::Mesh subMesh = lionModel.mMeshArray[i];

				MeshBatch* pMeshBatch = (MeshBatch*)conf_placement_new<MeshBatch>(conf_calloc(1, sizeof(MeshBatch)));

				sceneData.scene.push_back(pMeshBatch);

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
		}

		AssimpImporter::Model sphereModel;

		{
			if (!importer.ImportModel("../../../../art/Meshes/lowpoly/geosphere.obj", &sphereModel))
			{
				return false;
			}

			size_t meshSize = sphereModel.mMeshArray.size();

			AssimpImporter::Mesh subMesh = sphereModel.mMeshArray[0];

			MeshBatch* pMeshBatch = (MeshBatch*)conf_placement_new<MeshBatch>(conf_calloc(1, sizeof(MeshBatch)));

			sceneData.sphere = pMeshBatch;

			// Vertex Buffer
			BufferLoadDesc bufferDesc = {};
			bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
			bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			bufferDesc.mDesc.mVertexStride = sizeof(float3);
			bufferDesc.mDesc.mSize = bufferDesc.mDesc.mVertexStride * subMesh.mPositions.size();
			bufferDesc.pData = subMesh.mPositions.data();
			bufferDesc.ppBuffer = &pMeshBatch->pPositionStream;
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

	void removeMesh(MeshBatch* mesh)
	{
		removeResource(mesh->pPositionStream);
		removeResource(mesh->pIndicesStream);

		if (mesh->pUVStream)
			removeResource(mesh->pUVStream);
		if (mesh->pNormalStream)
			removeResource(mesh->pNormalStream);
	}
};

DEFINE_APPLICATION_MAIN(BloomHDR)