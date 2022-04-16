#include "app.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "materialsystem/ITexture.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "istudiorender.h"
#include "tier2/camerautils.h"

#include "studiomodel.h"

// Bring in our non-source things
#include "memdbgoff.h"

#include <imgui_impl_source.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/imgui.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32 1
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>


// Currently blank, but might be worth filling in if you need mat proxies
class CDummyMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	virtual IMaterialProxy *CreateProxy(const char *proxyName) { return nullptr; }
	virtual void DeleteProxy(IMaterialProxy *pProxy) { }
};
CDummyMaterialProxyFactory g_DummyMaterialProxyFactory;




void CImGuiSourceApp::Init()
{
	if (!glfwInit())
		return;

	// We're handled by matsys, no api
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_pWindow = glfwCreateWindow(640, 480, "Example Dear ImGui Source App", NULL, NULL);
	if (!m_pWindow)
		return;

	glfwMakeContextCurrent(m_pWindow);
#ifdef _WIN32
	HWND hwnd = glfwGetWin32Window(m_pWindow);
#endif

	// Set up matsys
	MaterialSystem_Config_t config;
	config = g_pMaterialSystem->GetCurrentConfigForVideoCard();
	config.SetFlag(MATSYS_VIDCFG_FLAGS_WINDOWED, true);
	config.SetFlag(MATSYS_VIDCFG_FLAGS_RESIZING, true);

	// Feed material system our window
	if (!g_pMaterialSystem->SetMode((void*)hwnd, config))
		return;
	g_pMaterialSystem->OverrideConfig(config, false);

	// We want to set this before we load up any mats, else it'll reload em all
	g_pMaterialSystem->SetMaterialProxyFactory(&g_DummyMaterialProxyFactory);

	// White out our cubemap and lightmap, as we don't have either
	m_pWhiteTexture = g_pMaterialSystem->FindTexture("white", NULL, true);
	m_pWhiteTexture->AddRef();
	g_pMaterialSystem->GetRenderContext()->BindLocalCubemap(m_pWhiteTexture);
	g_pMaterialSystem->GetRenderContext()->BindLightmapTexture(m_pWhiteTexture);

	// If we don't do this, all models will render black
	int samples = g_pStudioRender->GetNumAmbientLightSamples();
	m_ambientLightColors = new Vector[samples];
	for (int i = 0; i < samples; i++)
		m_ambientLightColors[i] = { 1,1,1 };
	g_pStudioRender->SetAmbientLightColors(m_ambientLightColors);
	
	// Init Dear ImGui
	ImGui::CreateContext();
	ImGui_ImplSource_Init();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOther(m_pWindow, true);
//	ImGui_ImplSourceGLFW_Init(m_pWindow, (void*)hwnd);

	m_lastFrameTime = glfwGetTime();

	// Main app loop
	while (!glfwWindowShouldClose(m_pWindow))
	{
		glfwPollEvents();
		ImGui_ImplGlfw_NewFrame();
		DrawFrame();
	}
}

void CImGuiSourceApp::Destroy()
{
	ImGui_ImplSource_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	// Clean up all of our assets, windows, etc
	if(m_ambientLightColors)
		delete[] m_ambientLightColors;

	if (m_pWhiteTexture)
		m_pWhiteTexture->DecrementReferenceCount();

	if(m_pWindow)
		glfwDestroyWindow(m_pWindow);
	glfwTerminate();
}

// Current model in use
static char s_modelName[256] = "models/barney.mdl";

void CImGuiSourceApp::DrawFrame()
{
	// What's our delta time?
	float curTime = glfwGetTime();
	float dt = curTime - m_lastFrameTime;
	m_lastFrameTime = curTime;

	// Start Frame
	g_pMaterialSystem->BeginFrame(0);

	// Clear out the old frame
	CMatRenderContextPtr ctx(g_pMaterialSystem);
	ctx->ClearColor3ub(0x30, 0x30, 0x30);
	ctx->ClearBuffers(true, true);

	// Let it know our window size
	int w, h;
	glfwGetWindowSize(m_pWindow, &w, &h);
	ctx->Viewport(0, 0, w, h);

	// Begin ImGui
	// Ideally this happens before we branch off into other functions, as it needs to be setup for other sections of code to use imgui
	ImGuiIO& io = ImGui::GetIO();
	ImGui::NewFrame();

	// Make us a nice camera
	VMatrix viewMatrix;
	VMatrix projMatrix;
	static float zoom = 120.0;
	static Camera_t cam = { {-zoom, 0, 0}, {0, 0, 0}, 65, 1.0f, 20000.0f };
	ComputeViewMatrix(&viewMatrix, cam);
	ComputeProjectionMatrix(&projMatrix, cam, w, h);

	// 3D Rendering mode
	ctx->MatrixMode(MATERIAL_PROJECTION);
	ctx->LoadMatrix(projMatrix);
	ctx->MatrixMode(MATERIAL_VIEW);
	ctx->LoadMatrix(viewMatrix);

	// Draw our model
	static CStudioModel* model = new CStudioModel(s_modelName);
	static QAngle ang = { 0, 0,0 };
	static Vector pos = -model->Center();
	model->m_time = curTime;
	model->m_sequence = 40;
	model->Draw(pos, ang);

	// Mouse input
	// If we're dragging a window, we don't want to be dragging our model too
	if (!io.WantCaptureMouse)
	{
		// Slow down our zoom as we get closer in for finer movements
		float mw = io.MouseWheel;
		mw *= zoom / 20.0f;
		
		// Don't allow zooming into and past our model
		zoom += mw;
		if (zoom <= 1)
			zoom = 1;

		// Camera rotation
		float x = io.MousePos.x;
		float y = io.MousePos.y;
		static float ox = 0, oy = 0;
		if (io.MouseDown[0])
		{
			cam.m_angles.y -= x - ox;
			cam.m_angles.x += y - oy;
		}
		ox = x;
		oy = y;

		// Set the camera to its new position
		Vector forward;
		AngleVectors(cam.m_angles, &forward);
		cam.m_origin = forward * -zoom;
	}

	// Model Properties
	if (ImGui::Begin("Model"))
	{
		ImGui::InputText("Path", s_modelName, sizeof(s_modelName));
		ImGui::SameLine();
		if (ImGui::Button("Apply"))
		{
			delete model;
			model = new CStudioModel(s_modelName);
		}
		
		ImGui::InputFloat3("pos", pos.Base());
		ImGui::SliderFloat3("ang", ang.Base(), -360, 360);
	}
	ImGui::End();

	ImGui::ShowDemoWindow();

	// End ImGui, and let it draw
	ImGui::Render();
	ImGui_ImplSource_RenderDrawData(ImGui::GetDrawData());

	// End Frame
	g_pMaterialSystem->SwapBuffers();
	g_pMaterialSystem->EndFrame();
}
