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
DepthState* pDepthStencilState = NULL;
DepthState* pDepthDefault = NULL;
DepthState* pDepthNone = NULL;

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
		Forward
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

struct
{
	Buffer* vertexBuffer;
	Buffer* indexBuffer;
	size_t indexCount;
	size_t vertexCount;
} quads;

struct
{
	struct
	{
		Texture* colorMap = NULL;
	} model;
} textures;

struct
{
	mat4	world;
	mat4	view;
	mat4	proj;
	float4	lightPos = { 0.0f, 2.0f, 2.0f, 0.0f };
	float outlineThickness;
} gUniformData;

Buffer* pUniformBuffers[gImageCount] = { NULL };

RenderPassMap	RenderPasses;

const char* pTexturesFileNames[] =
{
	"lion/lion_albedo",
	"lion/lion_specular"
};

const char* pszBases[FSR_Count] = {
	"../../../../src/Shaders/bin",													// FSR_BinShaders
	"../../../../src/08_ToonShading/",												// FSR_SrcShaders
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

		// Forward Pass
		RenderPassData* pass =
			conf_placement_new<RenderPassData>(conf_calloc(1, sizeof(RenderPassData)), pRenderer, pGraphicsQueue, gImageCount);
		RenderPasses.insert(eastl::pair<RenderPass::Enum, RenderPassData*>(RenderPass::Forward, pass));

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAquiredSemaphore);

		// Resource Loading
		initResourceLoaderInterface(pRenderer);

		// Initialize profile
		initProfiler(pRenderer);

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

		// Shader
		ShaderLoadDesc shaderDesc = {};
		shaderDesc.mStages[0] = { "toon.vert", NULL, 0, FSR_SrcShaders };
		shaderDesc.mStages[1] = { "toon.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &shaderDesc, &RenderPasses[RenderPass::Forward]->pShader);

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

			depthStateDesc = {};
			depthStateDesc.mDepthTest = false;
			depthStateDesc.mDepthWrite = false;
			depthStateDesc.mDepthFunc = CMP_ALWAYS;
			addDepthState(pRenderer, &depthStateDesc, &pDepthNone);

			depthStateDesc = {};
			addDepthState(pRenderer, &depthStateDesc, &pDepthStencilState);
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
			// Root Signature for Forward Pipeline
			{
				Shader* shaders[1] = { RenderPasses[RenderPass::Forward]->pShader };
				RootSignatureDesc rootDesc = {};
				rootDesc.mShaderCount = 1;
				rootDesc.ppShaders = shaders;
				rootDesc.mStaticSamplerCount = 1;
				rootDesc.ppStaticSamplerNames = &samplerNames;
				rootDesc.ppStaticSamplers = &pSampler;
				addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::Forward]->pRootSignature);
			}
		}

		DescriptorBinderDesc descriptorBinderDescs[1] =
		{
			{ RenderPasses[RenderPass::Forward]->pRootSignature }
		};

		addDescriptorBinder(pRenderer, 0, 1, descriptorBinderDescs, &pDescriptorBinder);

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
		}

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
			removeResource(pUniformBuffers[i]);
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
			removeResource(sceneData.meshes[i]->pNormalStream);
			removeResource(sceneData.meshes[i]->pIndicesStream);
			if (sceneData.meshes[i]->pUVStream)
				removeResource(sceneData.meshes[i]->pUVStream);
		}

		removeResource(textures.model.colorMap);

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
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 100.0f);

		gUniformData.view = viewMat;
		gUniformData.proj = projMat;

		// Update Instance Data
		gUniformData.world = mat4::translation(Vector3(0.0f, -1, 7)) *
			mat4::rotationY(currentTime) *
			mat4::scale(Vector3(0.5f));

		viewMat.setTranslation(vec3(0));

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
		BufferUpdateDesc viewProjCbv = { pUniformBuffers[gFrameIndex], &gUniformData };
		updateResource(&viewProjCbv);

		// Load Actions
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 0.0f;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;

		// Forward Pass
		Cmd* cmd = RenderPasses[RenderPass::Forward]->ppCmds[gFrameIndex];
		{
			RenderTarget* pSwapChainRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

			beginCmd(cmd);
			cmdBeginGpuFrameProfile(cmd, pGpuProfiler);
			{
				cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Forward Pass", true);

				TextureBarrier textureBarriers[2] = {
					{ pSwapChainRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
					{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE }
				};

				cmdResourceBarrier(cmd, 0, nullptr, 2, textureBarriers, false);

				cmdBindRenderTargets(cmd, 1, &pSwapChainRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
				cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSwapChainRenderTarget->mDesc.mWidth, (float)pSwapChainRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, pSwapChainRenderTarget->mDesc.mWidth, pSwapChainRenderTarget->mDesc.mHeight);

				{
					DescriptorData params[1] = {};
					params[0].pName = "UniformData";
					params[0].ppBuffers = &pUniformBuffers[gFrameIndex];

					cmdBindDescriptors(cmd, pDescriptorBinder, RenderPasses[RenderPass::Forward]->pRootSignature, 1, params);

					cmdBindPipeline(cmd, RenderPasses[RenderPass::Forward]->pPipeline);
					{
						for (int i = 0; i < sceneData.meshes.size(); ++i)
						{
							Buffer* pVertexBuffers[] = { sceneData.meshes[i]->pPositionStream, sceneData.meshes[i]->pNormalStream };
							cmdBindVertexBuffer(cmd, 2, pVertexBuffers, NULL);
							cmdBindIndexBuffer(cmd, sceneData.meshes[i]->pIndicesStream, 0);
							cmdDrawIndexed(cmd, sceneData.meshes[i]->mCountIndices, 0, 0);
						}
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
		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAquiredSemaphore, 1, &pRenderCompleteSemaphore);

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
		// Forward
		{
			VertexLayout vertexLayout = {};
			vertexLayout.mAttribCount = 2;

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

			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = pDepthDefault;
			pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
			pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
			pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
			pipelineSettings.pRootSignature = RenderPasses[RenderPass::Forward]->pRootSignature;
			pipelineSettings.pShaderProgram = RenderPasses[RenderPass::Forward]->pShader;
			pipelineSettings.pVertexLayout = &vertexLayout;
			pipelineSettings.pRasterizerState = pRasterDefault;

			addPipeline(pRenderer, &desc, &RenderPasses[RenderPass::Forward]->pPipeline);
		}
	}

	const char* GetName() { return "08_ToonShading"; }

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

		// Last component of position is used for debug display sampler index
		vertexBuffer.push_back({ { -1.0f,		+3.0f,		0.0f	}, { 0.0f, 0.0f } });
		vertexBuffer.push_back({ { +1.0f,		+3.0f,		0.0f	}, { 1.0f, 0.0f } });
		vertexBuffer.push_back({ { -1.0f,		+1.0f,		0.0f	}, { 0.0f, 1.0f } });
		vertexBuffer.push_back({ { +1.0f,		+1.0f,		0.0f	}, { 1.0f, 1.0f } });

		vertexBuffer.push_back({ { -1.0f,		+1.0f,		0.1f	}, { 0.0f, 0.0f } });
		vertexBuffer.push_back({ { +1.0f,		+1.0f,		0.1f	}, { 1.0f, 0.0f } });
		vertexBuffer.push_back({ { -1.0f,		-1.0f,		0.1f	}, { 0.0f, 1.0f } });
		vertexBuffer.push_back({ { +1.0f,		-1.0f,		0.1f	}, { 1.0f, 1.0f } });

		vertexBuffer.push_back({ { +1.0f,		+1.0f,		0.2f	}, { 0.0f, 0.0f } });
		vertexBuffer.push_back({ { +3.0f,		+1.0f,		0.2f	}, { 1.0f, 0.0f } });
		vertexBuffer.push_back({ { +1.0f,		-1.0f,		0.2f	}, { 0.0f, 1.0f } });
		vertexBuffer.push_back({ { +3.0f,		-1.0f,		0.2f	}, { 1.0f, 1.0f } });

		vertexBuffer.push_back({ { +1.0f,		+3.0f,		0.3f	}, { 0.0f, 0.0f } });
		vertexBuffer.push_back({ { +3.0f,		+3.0f,		0.3f	}, { 1.0f, 0.0f } });
		vertexBuffer.push_back({ { +1.0f,		+1.0f,		0.3f	}, { 0.0f, 1.0f } });
		vertexBuffer.push_back({ { +3.0f,		+1.0f,		0.3f	}, { 1.0f, 1.0f } });

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
		eastl::vector<uint32_t> indexBuffer;

		for (uint32_t i = 0; i < 4; ++i)
		{
			uint32_t indices[6] = { 0,2,1, 1,2,3 };
			for (auto index : indices)
			{
				indexBuffer.push_back(i * 4 + index);
			}
		}

		quads.indexCount = static_cast<uint32_t>(indexBuffer.size());

		BufferLoadDesc quadIndexBufferDesc = {};
		quadIndexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		quadIndexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		quadIndexBufferDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		quadIndexBufferDesc.mDesc.mSize = sizeof(uint32_t) * quads.indexCount;
		quadIndexBufferDesc.pData = indexBuffer.data();
		quadIndexBufferDesc.ppBuffer = &quads.indexBuffer;
		addResource(&quadIndexBufferDesc);
	}

	bool LoadModels()
	{
		// Main Texture
		TextureLoadDesc textureDesc = {};
		textureDesc.mRoot = FSR_Textures;
		textureDesc.pFilename = pTexturesFileNames[0];
		textureDesc.ppTexture = &textures.model.colorMap;
		addResource(&textureDesc, true);

		AssimpImporter importer;

		AssimpImporter::Model model;
		if (!importer.ImportModel("../../../../art/Meshes/chinesedragon.dae", &model))
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

			if (subMesh.mUvs.data())
			{
				bufferDesc.mDesc.mVertexStride = sizeof(float2);
				bufferDesc.mDesc.mSize = subMesh.mUvs.size() * bufferDesc.mDesc.mVertexStride;
				bufferDesc.pData = subMesh.mUvs.data();
				bufferDesc.ppBuffer = &pMeshBatch->pUVStream;
				addResource(&bufferDesc);
			}

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