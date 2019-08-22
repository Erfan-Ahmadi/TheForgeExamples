#include "../common.h"

const uint32_t	gImageCount = 3;
bool			bToggleMicroProfiler = false;
bool			bPrevToggleMicroProfiler = false;
uint32_t		gNumQuadPoints;

Renderer* pRenderer = NULL;

Queue* pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd** ppCmds = NULL;
Sampler* pSampler = NULL;
RasterizerState* pRastState = NULL;
DepthState* pDepthState = NULL;
RenderTarget* pDepthBuffer = NULL;

Fence* pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };
Semaphore* pImageAquiredSemaphore = NULL;

SwapChain* pSwapChain = NULL;
Pipeline* pGraphicsPipeline = NULL;
RootSignature* pRootSignature = NULL;

Shader* pQuadShader = NULL;
Texture* pQuadTexture = NULL;
Buffer* pQuadVertexBuffer = NULL;
Pipeline* pQuadPipeline = NULL;

uint32_t			gFrameIndex = 0;

DescriptorBinder* pDescriptorBinder = NULL;

UIApp              gAppUI;
GpuProfiler* pGpuProfiler = NULL;
ICameraController* pCameraController = NULL;

GuiComponent* pGui = NULL;

const char* pTexturesFileNames[] = { "Quad" };

const char* pszBases[FSR_Count] = {
	"../../../../../The-Forge/Examples_3/Unit_Tests/src/01_Transformations/",		// FSR_BinShaders
	"../../../../src/01_HelloQuad/",													// FSR_SrcShaders
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

class HelloQuad : public IApp
{
public:
	HelloQuad()
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
		ShaderLoadDesc quadShaderDesc = {};
		quadShaderDesc.mStages[0] = { "quad.vert", NULL, 0, FSR_SrcShaders };
		quadShaderDesc.mStages[1] = { "quad.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &quadShaderDesc, &pQuadShader);

		// Texture
		TextureLoadDesc textureDesc = {};
		textureDesc.mRoot = FSR_Textures;
		textureDesc.pFilename = pTexturesFileNames[0];
		textureDesc.ppTexture = &pQuadTexture;
		addResource(&textureDesc, true); // What is batch and SyncToken?

		// Sampler
		SamplerDesc samplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSampler);


		// Get VBuffer Data
		float* pPoints;
		meshes::generateQuadPoints(&pPoints, &gNumQuadPoints);

		uint64_t quadDataSize = 36 * sizeof(float);

		// Vertex Buffer
		BufferLoadDesc quadVBufferDesc = {};
		quadVBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		quadVBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		quadVBufferDesc.mDesc.mSize = quadDataSize;
		quadVBufferDesc.mDesc.mVertexStride = sizeof(float) * 6;
		quadVBufferDesc.pData = pPoints;
		quadVBufferDesc.ppBuffer = &pQuadVertexBuffer;
		addResource(&quadVBufferDesc);

		conf_free(pPoints);

		// Resource Binding
		Shader* shaders = { pQuadShader };
		const char* pStaticSamplers[] = { "uSampler0" };

		RootSignatureDesc rootDesc = {};
		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = &shaders;
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplers = &pSampler;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		DescriptorBinderDesc descriptorBinderDescs[1] = { {pRootSignature} };
		addDescriptorBinder(pRenderer, 0, 1, descriptorBinderDescs, &pDescriptorBinder);

		// Rasterizer State
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRastState);

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
		CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ 0 };

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

		removeResource(pQuadVertexBuffer);
		//removeResource(pQuadTexture);

		removeDescriptorBinder(pRenderer, pDescriptorBinder);

		//removeSampler(pRenderer, pSampler);
		removeShader(pRenderer, pQuadShader);
		removeRootSignature(pRenderer, pRootSignature);

		removeDepthState(pDepthState);
		removeRasterizerState(pRastState);

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
		vertexLayout.mAttribCount = 2;
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
		pipelineSettings.pShaderProgram = pQuadShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pRastState;
		addPipeline(pRenderer, &desc, &pQuadPipeline);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		gAppUI.Unload();

		removePipeline(pRenderer, pQuadPipeline);

		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTarget(pRenderer, pDepthBuffer);
	}

	void Update(float deltaTime)
	{
		pCameraController->update(deltaTime);
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

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].b = 1.0f;
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

			cmdBindPipeline(cmd, pQuadPipeline);
			{
				cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 0, nullptr);
				cmdBindVertexBuffer(cmd, 1, &pQuadVertexBuffer, NULL);
				cmdDraw(cmd, 6, 0);
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
		swapChainDesc.mEnableVsync = false;
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

	const char* GetName() { return "01_HelloQuad"; }

	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}
};

DEFINE_APPLICATION_MAIN(HelloQuad)