#include "../common.h"
#include "Common_3/ThirdParty/OpenSource/Nothings/stb_image.h"

constexpr size_t gInstanceCount = 1;
constexpr size_t gMaxInstanceCount = 8;
static_assert(gInstanceCount <= gMaxInstanceCount, "");

constexpr size_t gDirectionalLights = 1;
constexpr size_t gMaxDirectionalLights = 1;
static_assert(gDirectionalLights <= gMaxDirectionalLights, "");

constexpr size_t gMaxPointLights = 8;
constexpr size_t gMaxSpotLights = 8;

static_assert(gInstanceCount < gMaxInstanceCount, "");


const uint32_t	gImageCount = 3;
bool			bToggleMicroProfiler = false;
bool			bPrevToggleMicroProfiler = false;
uint32_t		gNumCubePoints;

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

Shader* pCubeShader = NULL;
Texture* pCubeTexture = NULL;
Texture* pCubeSpecularTexture = NULL;
Buffer* pCubeVertexBuffer = NULL;
Pipeline* pCubePipeline = NULL;
Pipeline* pCubePipeline2 = NULL;

uint32_t			gFrameIndex = 0;

DescriptorBinder* pDescriptorBinder = NULL;

UIApp              gAppUI;
GpuProfiler* pGpuProfiler = NULL;
ICameraController* pCameraController = NULL;

GuiComponent* pGui = NULL;

Buffer* pLightBuffer = NULL;
Buffer* pDirLightsBuffer = NULL;
Buffer* pUniformBuffers[gImageCount] = { NULL };

struct UniformBuffer
{
	mat4	view;
	mat4	proj;
	mat4	pToWorld[gMaxInstanceCount];
} uniformData;

struct DirectionalLight {
	float3 direction;
	float3 ambient;
	float3 diffuse;
	float3 specular;
};

DirectionalLight directionalLights[gMaxDirectionalLights];

struct LightBuffer
{
	int numDirectionalLights;
	float3 viewPos;
} lightData;


struct Vertex
{
	float3 position;
	float3 normal;
	float2 textCoord;
};

constexpr uint64_t cubeDataSize = 6 * 6 * sizeof(Vertex);

const char* pTexturesFileNames[] = { "Skybox_front5" };

const char* pszBases[FSR_Count] = {
	"../../../../../The-Forge/Examples_3/Unit_Tests/src/01_Transformations/",		// FSR_BinShaders
	"../../../../src/04_LightMapping/",												// FSR_SrcShaders
	"../../../../../The-Forge/Examples_3/Unit_Tests/UnitTestResources/",			// FSR_Textures
	"../../../../../The-Forge/Examples_3/Unit_Tests/UnitTestResources/",			// FSR_Meshes
	"../../../../../The-Forge/Examples_3/Unit_Tests/UnitTestResources/",			// FSR_Builtin_Fonts
	"../../../../../The-Forge/Examples_3/Unit_Tests/src/01_Transformations/",		// FSR_GpuConfig
	"",																				// FSR_Animation
	"",																				// FSR_Audio
	"",																				// FSR_OtherFiles
	"../../../../../The-Forge/Middleware_3/Text/",									// FSR_MIDDLEWARE_TEXT
	"../../../../../The-Forge/Middleware_3/UI/",									// FSR_MIDDLEWARE_UI
};

class PhongShading : public IApp
{
public:
	PhongShading()
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
		cubeShaderDesc.mStages[0] = { "cube.vert", NULL, 0, FSR_SrcShaders };
		cubeShaderDesc.mStages[1] = { "cube.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &cubeShaderDesc, &pCubeShader);

		// Main Texture
		int width, height, channels;
		uint8_t* raw_image = stbi_load("../../../../src/04_LightMapping/Textures/container2.png", &width, &height, &channels, 4);

		RawImageData raw_image_data = { raw_image, ImageFormat::RGBA8, (uint32_t)width, (uint32_t)height, 1, 1, 1 };

		TextureLoadDesc textureDesc = {};
		textureDesc.mRoot = FSR_Textures;
		textureDesc.pRawImageData = &raw_image_data;
		textureDesc.ppTexture = &pCubeTexture;
		addResource(&textureDesc, true);

		// Specular Texture
		raw_image = stbi_load("../../../../src/04_LightMapping/Textures/container2_specular.png", &width, &height, &channels, 4);
		textureDesc.pRawImageData = &raw_image_data;
		raw_image_data = { raw_image, ImageFormat::RGBA8, (uint32_t)width, (uint32_t)height, 1, 1, 1 };
		textureDesc.ppTexture = &pCubeSpecularTexture;
		addResource(&textureDesc, true);

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
		rootDesc.ppShaders = &pCubeShader;
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplers = &pSampler;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		DescriptorBinderDesc descriptorBinderDescs[1] = { { pRootSignature } };
		addDescriptorBinder(pRenderer, 0, 1, descriptorBinderDescs, &pDescriptorBinder);

		BufferLoadDesc bufferDesc = {};

		// Vertex Buffer
		Vertex* pVertices;
		getCubeVertexData(&pVertices);
		bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufferDesc.mDesc.mSize = cubeDataSize;
		bufferDesc.mDesc.mVertexStride = sizeof(Vertex);
		bufferDesc.pData = pVertices;
		bufferDesc.ppBuffer = &pCubeVertexBuffer;
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

		removeResource(pCubeVertexBuffer);
		removeResource(pCubeTexture);
		removeResource(pCubeSpecularTexture);

		removeDescriptorBinder(pRenderer, pDescriptorBinder);

		removeSampler(pRenderer, pSampler);
		removeShader(pRenderer, pCubeShader);
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
		pipelineSettings.pShaderProgram = pCubeShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pRastState;
		addPipeline(pRenderer, &desc, &pCubePipeline);

		pipelineSettings.pRasterizerState = pSecondRastState;
		addPipeline(pRenderer, &desc, &pCubePipeline2);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		gAppUI.Unload();

		removePipeline(pRenderer, pCubePipeline);

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
		uniformData.pToWorld[0] = mat4::translation(Vector3(0, 0, 5));

		//mat4 rotateAroundPoint =
		//	mat4::translation(Vector3(0, 0, 5)) *
		//	mat4::rotationY(currentTime) * mat4::translation(Vector3(0, 0, -5)) *
		//	mat4::translation(Vector3(0, 2, 3));

		//uniformData.pToWorld[1] = rotateAroundPoint * mat4::scale(Vector3(0.4f));

		lightData.numDirectionalLights = 1;
		directionalLights[0].direction = float3{ 0.0f, -1.0f, 1.0f };
		directionalLights[0].ambient = float3{ 0.1f, 0.1f, 0.1f };
		directionalLights[0].diffuse = float3{ 1.0f, 1.0f, 1.0f };
		directionalLights[0].specular = float3{ 0.3f, 0.3f, 0.3f };
		lightData.numDirectionalLights = gDirectionalLights;
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
		BufferUpdateDesc lightCbv = { pLightBuffer, &lightData };
		updateResource(&lightCbv);
		
		// Update light uniform buffers
		BufferUpdateDesc dirLights = { pDirLightsBuffer, &directionalLights};
		updateResource(&dirLights);

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

			DescriptorData params[5] = {};
			params[0].pName = "Texture";
			params[0].ppTextures = &pCubeTexture;
			params[1].pName = "UniformData";
			params[1].ppBuffers = &pUniformBuffers[gFrameIndex];
			params[2].pName = "LightData";
			params[2].ppBuffers = &pLightBuffer;
			params[3].pName = "TextureSpecular";
			params[3].ppTextures = &pCubeSpecularTexture;
			params[4].pName = "DirectionalLights";
			params[4].ppBuffers = &pDirLightsBuffer;

			cmdBindPipeline(cmd, pCubePipeline);
			{
				cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 5, params);
				cmdBindVertexBuffer(cmd, 1, &pCubeVertexBuffer, NULL);
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

	const char* GetName() { return "04_LightMapping"; }

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

	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}

	static void getCubeVertexData(Vertex** vertexData)
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

DEFINE_APPLICATION_MAIN(PhongShading)