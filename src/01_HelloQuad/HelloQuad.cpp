#include "Common_3/OS/Interfaces/ICameraController.h"
#include "Common_3/OS/Interfaces/IApp.h"
#include "Common_3/Renderer/IRenderer.h"
#include "Common_3/Renderer/ResourceLoader.h"

//Input
#include "Common_3/OS/Input/InputSystem.h"
#include "Common_3/OS/Input/InputMappings.h"

//Math
#include "Common_3/OS/Math/MathTypes.h"

//ui
#include "Middleware_3/UI/AppUI.h"

const uint32_t gImageCount = 3;
bool           bToggleMicroProfiler = false;
bool           bPrevToggleMicroProfiler = false;

Renderer* pRenderer = NULL;

Queue* pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd** ppCmds = NULL;
Sampler* pSampler = NULL;
RasterizerState* pRastState = NULL;
DepthState* pDepthState = NULL;

Fence* pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };
Semaphore* pImageAquiredSemaphore = NULL;

SwapChain* swapChain = NULL;
Pipeline* pGraphicsPipeline = NULL;
RootSignature* pRootSignature = NULL;

Shader* pQuadShader = NULL;
Texture* pQuadTexture = NULL;
Buffer* pQuadVertexBuffer = NULL;

uint32_t			gFrameIndex = 0;

DescriptorBinder* pDescriptorBinder = NULL;

UIApp              gAppUI;
GpuProfiler* pGpuProfiler = NULL;
ICameraController* pCameraController = NULL;

GuiComponent* pGui = NULL;

const char* pTexturesFileNames[] = { "Quad" };

//TODO: Correct Paths
const char* pszBases[FSR_Count] = {
	"../../../src/01_Transformations/",     // FSR_BinShaders
	"../../../src/01_Transformations/",     // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/01_Transformations/",     // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_Audio
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
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
		
		// Vertex Buffer
		BufferLoadDesc quadVBufferDesc = {};
		quadVBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		quadVBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		//quadVBufferDesc.mDesc.mSize = size;
		//quadVBufferDesc.mDesc.mVertexStride = stride;
		//quadVBufferDesc.pData = data;
		quadVBufferDesc.ppBuffer = &pQuadVertexBuffer;
		addResource(&quadVBufferDesc);

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
    guiDesc.mStartPosition = vec2( mSettings.mWidth - guiDesc.mStartSize.getX() * 1.1f, guiDesc.mStartSize.getY() * 0.5f);

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
		removeResource(pQuadTexture);

		removeSampler(pRenderer, pSampler);
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
	}

	void Unload()
	{
	}

	void Update()
	{
	}

	void Draw()
	{
	}

	const char* GetName() { return "01_HelloQuad"; }
	
	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}
};