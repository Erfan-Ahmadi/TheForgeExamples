#include "../common.h"

#include "Common_3/OS/Interfaces/IProfiler.h"
#include "Middleware_3/UI/AppUI.h"

//EASTL includes
#include "Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "Common_3/ThirdParty/OpenSource/EASTL/string.h"

//asimp importer
#include "Common_3/Tools/AssimpImporter/AssimpImporter.h"
#include "Common_3/OS/Interfaces/IMemory.h"

constexpr size_t gPointLights = 1;
constexpr size_t gMaxPointLights = 8;
static_assert(gPointLights <= gMaxPointLights, "");

const uint32_t	gImageCount = 3;

Renderer* pRenderer = NULL;

Queue* pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd** ppCmds = NULL;

Fence* pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };
Semaphore* pImageAquiredSemaphore = NULL;
Semaphore* pFirstPassFinishedSemaphore = { NULL };

SwapChain* pSwapChain = NULL;
RootSignature* pRootSignature = NULL;

struct
{
	Pipeline* pPipeline = NULL;
	Shader* pShader = NULL;
	RasterizerState* pRastState = NULL;
	DepthState* pDepthState = NULL;
	RenderTarget* pDepthBuffer = NULL;
} firstPipeline, secondPipeline;

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

Buffer* pUniformBuffers[gImageCount] = { NULL };

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

struct UniformBuffer
{
	mat4	view;
	mat4	proj;
	mat4	pToWorld;
} uniformData;

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

		// Initialize profile
		initProfiler(pRenderer);

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

		// Shader
		ShaderLoadDesc cubeShaderDesc = {};
		cubeShaderDesc.mStages[0] = { "basic.vert", NULL, 0, FSR_SrcShaders };
		cubeShaderDesc.mStages[1] = { "basic.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &cubeShaderDesc, &firstPipeline.pShader);

		// Resource Binding
		RootSignatureDesc rootDesc = {};
		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = &firstPipeline.pShader;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		DescriptorBinderDesc descriptorBinderDescs[1] = { { pRootSignature } };
		addDescriptorBinder(pRenderer, 0, 1, descriptorBinderDescs, &pDescriptorBinder);

		BufferLoadDesc bufferDesc = {};

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

		if (!LoadModels())
		{
			finishResourceLoading();
			return false;
		}

		// Rasterizer State
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &firstPipeline.pRastState);

		// Depth State
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &firstPipeline.pDepthState);

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

		for (size_t i = 0; i < sceneData.meshes.size(); ++i)
		{
			removeResource(sceneData.meshes[i]->pPositionStream);
			removeResource(sceneData.meshes[i]->pUVStream);
			removeResource(sceneData.meshes[i]->pNormalStream);
			removeResource(sceneData.meshes[i]->pIndicesStream);
		}

		removeDescriptorBinder(pRenderer, pDescriptorBinder);

		removeShader(pRenderer, firstPipeline.pShader);
		removeDepthState(firstPipeline.pDepthState);
		removeRasterizerState(firstPipeline.pRastState);

		removeRootSignature(pRenderer, pRootSignature);

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

		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;

		loadProfiler(pSwapChain->ppSwapchainRenderTargets[0]);

		//layout and pipeline for sphere draw
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 1;

		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = firstPipeline.pDepthState;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = firstPipeline.pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = firstPipeline.pShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = firstPipeline.pRastState;
		addPipeline(pRenderer, &desc, &firstPipeline.pPipeline);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		unloadProfiler();
		gAppUI.Unload();

		gVirtualJoystick.Unload();

		removePipeline(pRenderer, firstPipeline.pPipeline);

		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTarget(pRenderer, firstPipeline.pDepthBuffer);
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

		uniformData.view = viewMat;
		uniformData.proj = projMat;

		// Update Instance Data
		uniformData.pToWorld = mat4::translation(Vector3(0.0f, -1, 5)) *
			mat4::rotationY(currentTime) *
			mat4::scale(Vector3(1.5f));

		viewMat.setTranslation(vec3(0));

		// ProfileSetDisplayMode()
		// TODO: need to change this better way 
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
		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		TextureBarrier textureBarriers[2] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ firstPipeline.pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE }
		};

		cmdResourceBarrier(cmd, 0, nullptr, 2, textureBarriers, false);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, firstPipeline.pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);


		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Scene", true);
		{
			DescriptorData params[1] = {};
			params[0].pName = "UniformData";
			params[0].ppBuffers = &pUniformBuffers[gFrameIndex];

			cmdBindPipeline(cmd, firstPipeline.pPipeline);
			{
				cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 1, params);
				Buffer* pVertexBuffers[] = { sceneData.meshes[0]->pPositionStream };
				cmdBindVertexBuffer(cmd, 1, pVertexBuffers, NULL);

				cmdBindIndexBuffer(cmd, sceneData.meshes[0]->pIndicesStream, 0);

				cmdDrawIndexed(cmd, sceneData.meshes[0]->mCountIndices, 0, 0);
			}
		}
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
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

		textureBarriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, true);

		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAquiredSemaphore, 1, &pRenderCompleteSemaphore);
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
		::addRenderTarget(pRenderer, &depthRT, &firstPipeline.pDepthBuffer);

		return firstPipeline.pDepthBuffer != NULL;
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

	bool LoadModels()
	{
		AssimpImporter importer;

		AssimpImporter::Model model;
		if (!importer.ImportModel("../../../../art/Meshes/bunny.obj", &model))
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