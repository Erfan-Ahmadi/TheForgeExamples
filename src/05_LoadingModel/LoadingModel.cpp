#include "../common.h"

//EASTL includes
#include "Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "Common_3/ThirdParty/OpenSource/EASTL/string.h"

//asimp importer
#include "Common_3/Tools/AssimpImporter/AssimpImporter.h"
#include "Common_3/OS/Interfaces/IMemory.h"

constexpr size_t gInstanceCount = 5;
constexpr size_t gMaxInstanceCount = 8;
static_assert(gInstanceCount <= gMaxInstanceCount, "");

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
bool			bToggleMicroProfiler = false;
bool			bPrevToggleMicroProfiler = false;
uint32_t		gNumModelPoints;

Renderer* pRenderer = NULL;

Queue* pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd** ppCmds = NULL;
Sampler* pSampler = NULL;
RasterizerState* pRastState = NULL;
RasterizerState* pSecondRastState = NULL;
DepthState* pDepthState = NULL;
RenderTarget* pDepthBuffer = NULL;

Fence* pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };
Semaphore* pImageAquiredSemaphore = NULL;

SwapChain* pSwapChain = NULL;
Pipeline* pGraphicsPipeline = NULL;
RootSignature* pRootSignature = NULL;

Shader* pModelShader = NULL;
Texture* pTexture = NULL;
Texture* pSpecularTexture = NULL;
Buffer* pModelVertexBuffer = NULL;
Pipeline* pModelPipeline = NULL;
Pipeline* pModelPipeline2 = NULL;

uint32_t			gFrameIndex = 0;

DescriptorBinder* pDescriptorBinder = NULL;

UIApp              gAppUI;
GpuProfiler* pGpuProfiler = NULL;
ICameraController* pCameraController = NULL;

GuiComponent* pGui = NULL;

Buffer* pUniformBuffers[gImageCount] = { NULL };

struct MeshBatch
{
	Buffer* pPositionStream;
	Buffer* pNormalStream;
	Buffer* pUVStream;
	Buffer* pIndicesStream;
};

struct SceneData
{
	eastl::vector<MeshBatch*> meshes;
} sceneData;

struct UniformBuffer
{
	mat4	view;
	mat4	proj;
	mat4	pToWorld[gMaxInstanceCount];
} uniformData;

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

Buffer* pDirLightsBuffer = NULL;
Buffer* pPointLightsBuffer = NULL;
Buffer* pSpotLightsBuffer = NULL;

DirectionalLight	directionalLights[gDirectionalLights];
PointLight			pointLights[gPointLights];
SpotLight			spotLights[gSpotLights];

struct LightBuffer
{
	int numDirectionalLights;
	int numPointLights;
	int numSpotLights;
	alignas(16) float3 viewPos;
} lightData;

Buffer* pLightBuffer = NULL;

struct Vertex
{
	float3 position;
	float3 normal;
	float2 textCoord;
};

const char* pTexturesFileNames[] =
{
	"lion/lion_albedo",
	"lion/lion_specular"
};

const char* pszBases[FSR_Count] = {
	"../../../../../The-Forge/Examples_3/Unit_Tests/src/01_Transformations/",		// FSR_BinShaders
	"../../../../src/05_LoadingModel/",												// FSR_SrcShaders
	"../../../../src/05_LoadingModel/",												// FSR_Textures
	"../../../../../The-Forge/Examples_3/Unit_Tests/UnitTestResources/",			// FSR_Meshes
	"../../../../../The-Forge/Examples_3/Unit_Tests/UnitTestResources/",			// FSR_Builtin_Fonts
	"../../../../../The-Forge/Examples_3/Unit_Tests/src/01_Transformations/",		// FSR_GpuConfig
	"",																				// FSR_Animation
	"",																				// FSR_Audio
	"",																				// FSR_OtherFiles
	"../../../../../The-Forge/Middleware_3/Text/",									// FSR_MIDDLEWARE_TEXT
	"../../../../../The-Forge/Middleware_3/UI/",									// FSR_MIDDLEWARE_UI
};

AssimpImporter::Model gModel;

class LoadingModel : public IApp
{
public:
	LoadingModel()
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
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAquiredSemaphore);

		// Resource Loading
		initResourceLoaderInterface(pRenderer);

		// Shader
		ShaderLoadDesc cubeShaderDesc = {};
		cubeShaderDesc.mStages[0] = { "basic.vert", NULL, 0, FSR_SrcShaders };
		cubeShaderDesc.mStages[1] = { "basic.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &cubeShaderDesc, &pModelShader);

		// Sampler
		SamplerDesc samplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSampler);

		// Resource Binding
		const char* pStaticSamplers[] = { "uSampler0" };

		RootSignatureDesc rootDesc = {};
		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = &pModelShader;
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplers = &pSampler;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		DescriptorBinderDesc descriptorBinderDescs[1] = { { pRootSignature } };
		addDescriptorBinder(pRenderer, 0, 1, descriptorBinderDescs, &pDescriptorBinder);

		BufferLoadDesc bufferDesc = {};

		// Vertex Buffer
		Vertex* pVertices;
		getModelVertexData(&pVertices);
		bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufferDesc.mDesc.mSize = 6 * 6 * sizeof(Vertex);
		bufferDesc.mDesc.mVertexStride = sizeof(Vertex);
		bufferDesc.pData = pVertices;
		bufferDesc.ppBuffer = &pModelVertexBuffer;
		addResource(&bufferDesc);

		// Uniform Buffer
		bufferDesc = {};
		bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufferDesc.mDesc.mSize = sizeof(UniformBuffer);
		bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufferDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			bufferDesc.ppBuffer = &pUniformBuffers[i];
			addResource(&bufferDesc);
		}

		CreateLightsBuffer();

		if (!LoadModels())
		{
			finishResourceLoading();
			return false;
		}

		// Rasterizer State
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRastState);

		// Rasterizer State
		rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pSecondRastState);

		// Depth State
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepthState);

		finishResourceLoading();

		// GUI
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		GuiDesc guiDesc = {};
		float   dpiScale = getDpiScale().x;
		guiDesc.mStartSize = vec2(140.0f / dpiScale, 320.0f / dpiScale);
		guiDesc.mStartPosition = vec2(mSettings.mWidth - guiDesc.mStartSize.getX() * 1.1f, guiDesc.mStartSize.getY() * 0.5f);

		pGui = gAppUI.AddGuiComponent("Micro profiler", &guiDesc);

		pGui->AddWidget(CheckboxWidget("Toggle Micro Profiler", &bToggleMicroProfiler));

		// Camera
		CameraMotionParameters cmp{ 40.0f, 30.0f, 100.0f };
		vec3                   camPos{ 0.0f, 0.0f, 0.8f };
		vec3                   lookAt{ 0.0f, 0.0f, 5.0f };

		pCameraController = createFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);
#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif
		InputSystem::RegisterInputEvent(cameraInputEvent);
		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);
		destroyCameraController(pCameraController);

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pUniformBuffers[i]);
		}

		removeResource(pLightBuffer);
		removeResource(pPointLightsBuffer);
		removeResource(pDirLightsBuffer);
		removeResource(pSpotLightsBuffer);

		removeResource(pModelVertexBuffer);
		removeResource(pTexture);
		removeResource(pSpecularTexture);

		removeDescriptorBinder(pRenderer, pDescriptorBinder);

		removeSampler(pRenderer, pSampler);
		removeShader(pRenderer, pModelShader);
		removeRootSignature(pRenderer, pRootSignature);

		removeDepthState(pDepthState);
		removeRasterizerState(pSecondRastState);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;
		if (!addDepthBuffer())
			return false;
		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		//layout and pipeline for sphere draw
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 3;

		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayout.mAttribs[1].mFormat = ImageFormat::RGB32F;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

		vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[2].mFormat = ImageFormat::RG32F;
		vertexLayout.mAttribs[2].mBinding = 0;
		vertexLayout.mAttribs[2].mLocation = 2;
		vertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthState;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pModelShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pRastState;
		addPipeline(pRenderer, &desc, &pModelPipeline);

		pipelineSettings.pRasterizerState = pSecondRastState;
		addPipeline(pRenderer, &desc, &pModelPipeline2);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		gAppUI.Unload();

		removePipeline(pRenderer, pModelPipeline);

		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTarget(pRenderer, pDepthBuffer);
	}

	void Update(float deltaTime)
	{
		static float currentTime;
		currentTime += deltaTime;

		pCameraController->update(deltaTime);

		// update camera with time
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

		uniformData.view = viewMat;
		uniformData.proj = projMat;

		// Update Instance Data
		for (uint32_t i = 0; i < gInstanceCount; ++i)
		{
			uniformData.pToWorld[i] = mat4::translation(Vector3(-4.0f + 2.0f * i, -1, 5)) *
				mat4::rotationX(i % 2 * currentTime) *
				mat4::rotationY((i) % 3 * currentTime);
		}

		directionalLights[0].direction = float3{ 0.0f, -1.0f, 0.0f };
		directionalLights[0].ambient = float3{ 0.05f, 0.05f, 0.05f };
		directionalLights[0].diffuse = float3{ 0.5f, 0.5f, 0.5f };
		directionalLights[0].specular = float3{ 0.5f, 0.5f, 0.5f };
		lightData.numDirectionalLights = 0;

		pointLights[0].position = float3{ -4.0f, 4.0f, 5.0f };
		pointLights[0].ambient = float3{ 0.1f, 0.1f, 0.1f };
		pointLights[0].diffuse = float3{ 1.0f, 1.0f, 1.0f };
		pointLights[0].specular = float3{ 1.0f, 1.0f, 1.0f };
		pointLights[0].attenuationParams = float3{ 1.0f, 0.07f, 0.017f };
		lightData.numPointLights = 0;

		spotLights[0].position = float3{ 4.0f * sin(currentTime), 0.0f, 0.0f };
		spotLights[0].direction = float3{ -0.5f, 0.0f, 1.0f };
		spotLights[0].cutOffs = float2{ cos(PI / 9), cos(PI / 6) };
		spotLights[0].ambient = float3{ 0.1f, 0.1f, 0.1f };
		spotLights[0].diffuse = float3{ 1.0f, 1.0f, 1.0f };
		spotLights[0].specular = float3{ 1.0f, 1.0f, 1.0f };
		spotLights[0].attenuationParams = float3{ 1.0f, 0.07f, 0.017f };
		lightData.numSpotLights = gSpotLights;

		lightData.viewPos = v3ToF3(pCameraController->getViewPosition());

		viewMat.setTranslation(vec3(0));
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAquiredSemaphore, NULL, &gFrameIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		// Update uniform buffers
		BufferUpdateDesc viewProjCbv = { pUniformBuffers[gFrameIndex], &uniformData };
		updateResource(&viewProjCbv);

		// Update light uniform buffers
		BufferUpdateDesc lightBuffUpdate = { pLightBuffer, &lightData };
		updateResource(&lightBuffUpdate);

		BufferUpdateDesc pointLightBuffUpdate = { pPointLightsBuffer, &pointLights };
		updateResource(&pointLightBuffUpdate);

		BufferUpdateDesc dirLightBuffUpdate = { pDirLightsBuffer, &directionalLights };
		updateResource(&dirLightBuffUpdate);

		BufferUpdateDesc spotLightBuffUpdate = { pSpotLightsBuffer, &spotLights };
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

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);
		{
			TextureBarrier textureBarriers[2] = {
				{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE }
			};

			cmdResourceBarrier(cmd, 0, nullptr, 2, textureBarriers, false);

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			DescriptorData params[7] = {};
			params[0].pName = "Texture";
			params[0].ppTextures = &pTexture;
			params[1].pName = "TextureSpecular";
			params[1].ppTextures = &pSpecularTexture;
			params[2].pName = "UniformData";
			params[2].ppBuffers = &pUniformBuffers[gFrameIndex];
			params[3].pName = "LightData";
			params[3].ppBuffers = &pLightBuffer;
			params[4].pName = "DirectionalLights";
			params[4].ppBuffers = &pDirLightsBuffer;
			params[5].pName = "PointLights";
			params[5].ppBuffers = &pPointLightsBuffer;
			params[6].pName = "SpotLights";
			params[6].ppBuffers = &pSpotLightsBuffer;

			cmdBindPipeline(cmd, pModelPipeline);
			{
				cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 7, params);
				cmdBindVertexBuffer(cmd, 1, &pModelVertexBuffer, NULL);
				cmdDrawInstanced(cmd, 6 * 6, 0, gInstanceCount, 0);
			}

			textureBarriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
			cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, true);

		}
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
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

	bool addDepthBuffer()
	{
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 1.0f;
		depthRT.mClearValue.stencil = 0.0f;
		depthRT.mFormat = ImageFormat::D32F;
		depthRT.mDepth = 1;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		::addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

	const char* GetName() { return "05_LoadingModel"; }

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

	void CreateLightsBuffer()
	{
		BufferLoadDesc bufferDesc = {};

		// Light Uniform Buffer
		bufferDesc = {};
		bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufferDesc.mDesc.mSize = sizeof(LightBuffer);
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
		bufferDesc.ppBuffer = &pDirLightsBuffer;
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
		bufferDesc.ppBuffer = &pPointLightsBuffer;
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
		bufferDesc.ppBuffer = &pSpotLightsBuffer;
		addResource(&bufferDesc);
	}

	bool LoadModels()
	{
		// Main Texture
		TextureLoadDesc textureDesc = {};
		textureDesc.mRoot = FSR_Textures;
		textureDesc.pFilename = pTexturesFileNames[0];
		textureDesc.ppTexture = &pTexture;
		addResource(&textureDesc, true);

		// Specular Texture
		textureDesc.pFilename = pTexturesFileNames[1];
		textureDesc.ppTexture = &pSpecularTexture;
		addResource(&textureDesc, true);

		AssimpImporter importer;

		if (!importer.ImportModel("../../../../src/04_LightMapping/Meshes/lion.obj", &gModel))
		{
			return false;
		}

		size_t meshSize = gModel.mMeshArray.size();

		for (size_t i = 0; i < meshSize; ++i)
		{
			AssimpImporter::Mesh subMesh = gModel.mMeshArray[i];

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

	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}

	static void getModelVertexData(Vertex** vertexData)
	{
		Vertex* pVertices = (Vertex*)conf_malloc(sizeof(Vertex) * 6 * 6);

		//		   .4------5     4------5     4------5     4------5     4------5.
		//		 .' |    .'|    /|     /|     |      |     |\     |\    |`.    | `.
		//		0---+--1'  |   0-+----1 |     0------1     | 0----+-1   |  `0--+---1
		//		|   |  |   |   | |    | |     |      |     | |    | |   |   |  |   |
		//		|  ,6--+---7   | 6----+-7     6------7     6-+----7 |   6---+--7   |
		//		|.'    | .'    |/     |/      |      |      \|     \|    `. |   `. |
		//		2------3'      2------3       2------3       2------3      `2------3

		float3 point0 = float3{ -0.5f, +0.5f, -0.5f };
		float3 point1 = float3{ +0.5f, +0.5f, -0.5f };
		float3 point2 = float3{ -0.5f, -0.5f, -0.5f };
		float3 point3 = float3{ +0.5f, -0.5f, -0.5f };
		float3 point4 = float3{ -0.5f, +0.5f, +0.5f };
		float3 point5 = float3{ +0.5f, +0.5f, +0.5f };
		float3 point6 = float3{ -0.5f, -0.5f, +0.5f };
		float3 point7 = float3{ +0.5f, -0.5f, +0.5f };

		float3 right = float3{ +1.0f, 0.0f, 0.0f };
		float3 left = float3{ -1.0f, 0.0f, 0.0f };
		float3 forward = float3{ 0.0f, 0.0f, +1.0f };
		float3 backward = float3{ 0.0f, 0.0f, -1.0f };
		float3 up = float3{ 0.0f, +1.0f, 0.0f };
		float3 down = float3{ 0.0f, -1.0f, 0.0f };

		float2 top_left = float2{ 0.0f, 0.0f };
		float2 top_right = float2{ 1.0f, 0.0f };
		float2 bottom_left = float2{ 0.0f, 1.0f };
		float2 bottom_right = float2{ 1.0f, 1.0f };

		// Front Face -> 0 1 3, 2 0 3
		pVertices[0] = Vertex{ point0, backward, top_left };
		pVertices[1] = Vertex{ point1, backward, top_right };
		pVertices[2] = Vertex{ point3, backward, bottom_right };
		pVertices[3] = Vertex{ point2, backward, bottom_left };
		pVertices[4] = Vertex{ point0, backward, top_left };
		pVertices[5] = Vertex{ point3, backward, bottom_right };

		// Back Face -> 5 4 7, 7 4 6
		pVertices[6] = Vertex{ point5, forward, top_left };
		pVertices[7] = Vertex{ point4, forward, top_right };
		pVertices[8] = Vertex{ point7, forward, bottom_left };
		pVertices[9] = Vertex{ point7, forward, bottom_left };
		pVertices[10] = Vertex{ point4, forward, top_right };
		pVertices[11] = Vertex{ point6, forward, bottom_right };

		// Right Face -> 1 5 3, 3 5 7
		pVertices[12] = Vertex{ point1, right, top_left };
		pVertices[13] = Vertex{ point5, right, top_right };
		pVertices[14] = Vertex{ point3, right, bottom_left };
		pVertices[15] = Vertex{ point3, right, bottom_left };
		pVertices[16] = Vertex{ point5, right, top_right };
		pVertices[17] = Vertex{ point7, right, bottom_right };

		// Left Face -> 4 0 6, 6 0 2
		pVertices[18] = Vertex{ point4, left, top_left };
		pVertices[19] = Vertex{ point0, left, top_right };
		pVertices[20] = Vertex{ point6, left, bottom_left };
		pVertices[21] = Vertex{ point6, left, bottom_left };
		pVertices[22] = Vertex{ point0, left, top_right };
		pVertices[23] = Vertex{ point2, left, bottom_right };

		// Top Face -> 4 5 0, 0 5 1
		pVertices[24] = Vertex{ point4, up, top_left };
		pVertices[25] = Vertex{ point5, up, top_right };
		pVertices[26] = Vertex{ point0, up, bottom_left };
		pVertices[27] = Vertex{ point0, up, bottom_left };
		pVertices[28] = Vertex{ point5, up, top_right };
		pVertices[29] = Vertex{ point1, up, bottom_right };

		// Bottom Face -> 7 6 3, 3 6 2
		pVertices[30] = Vertex{ point7, down, top_left };
		pVertices[31] = Vertex{ point6, down, top_right };
		pVertices[32] = Vertex{ point3, down, bottom_left };
		pVertices[33] = Vertex{ point3, down, bottom_left };
		pVertices[34] = Vertex{ point6, down, top_right };
		pVertices[35] = Vertex{ point2, down, bottom_right };
		*vertexData = pVertices;
	}
};

DEFINE_APPLICATION_MAIN(LoadingModel)