#include "app.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "materialsystem/ITexture.h"
#include "materialsystem/MaterialSystem_Config.h"

// Bring in our non-source things
#include "memdbgoff.h"

#include <imgui_impl_source.h>
#include <imgui/imgui.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32 1
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

class CDummyMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	virtual IMaterialProxy *CreateProxy(const char *proxyName) { return nullptr; }
	virtual void DeleteProxy(IMaterialProxy *pProxy) { }
};
CDummyMaterialProxyFactory g_DummyMaterialProxyFactory;


static const char* getClipboardText(void* user_data)
{
	return glfwGetClipboardString((GLFWwindow*)user_data);
}
static void setClipboardText(void* user_data, const char* text)
{
	glfwSetClipboardString((GLFWwindow*)user_data, text);
}
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	ImGuiIO& io = ImGui::GetIO();
	if (key >= 0 && key < IM_ARRAYSIZE(io.KeysDown))
		io.KeysDown[key] = action != GLFW_RELEASE;

	io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
	io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
	io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
	io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
}
static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	ImGuiIO& io = ImGui::GetIO();
	if (button >= 0 && button < IM_ARRAYSIZE(io.MouseDown))
		io.MouseDown[button] = action != GLFW_RELEASE;
}
static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MousePos = ImVec2((float)xpos, (float)ypos);
}
static void charCallback(GLFWwindow* window, unsigned int c)
{
	ImGuiIO& io = ImGui::GetIO();
	io.AddInputCharacter(c);
}
static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseWheelH += (float)xoffset;
	io.MouseWheel += (float)yoffset;
}



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

	// Init Dear ImGui
	ImGui::CreateContext();
	ImGui_ImplSource_Init();
	ImGui::StyleColorsDark();

	// Wire up Dear ImGui into glfw
	ImGuiIO& io = ImGui::GetIO();
	
	// Looks crazy, I know, but ImGui only wants these. It takes in the rest of the keys as "char inputs"
	io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
	io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
	io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
	io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
	io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
	io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
	io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
	io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
	io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
	io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
	io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
	io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
	io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
	io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
	io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
	io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
	io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

	io.SetClipboardTextFn = setClipboardText;
	io.GetClipboardTextFn = getClipboardText;
	io.ClipboardUserData = m_pWindow;
#if defined(_WIN32)
	io.ImeWindowHandle = (void*)hwnd;
#endif

	// Setup all our glfw callbacks
	glfwSetKeyCallback(        m_pWindow, keyCallback);
	glfwSetMouseButtonCallback(m_pWindow, mouseButtonCallback);
	glfwSetCursorPosCallback(  m_pWindow, cursorPosCallback);
	glfwSetCharCallback(       m_pWindow, charCallback);
	glfwSetScrollCallback(     m_pWindow, scrollCallback);
	
	m_lastFrameTime = glfwGetTime();

	// Main app loop
	while (!glfwWindowShouldClose(m_pWindow))
	{
		glfwPollEvents();
		Draw();
	}
}

void CImGuiSourceApp::Destroy()
{
	// Clean up all of our assets, windows, etc
	if (m_pWhiteTexture)
		m_pWhiteTexture->DecrementReferenceCount();

	if(m_pWindow)
		glfwDestroyWindow(m_pWindow);
	glfwTerminate();
}


void CImGuiSourceApp::Draw()
{
	float curTime = glfwGetTime();
	float dt = curTime - m_lastFrameTime;
	m_lastFrameTime = curTime;

	// Start Frame
	g_pMaterialSystem->BeginFrame(0);

	CMatRenderContextPtr pRenderContext(g_pMaterialSystem);
	pRenderContext->ClearColor3ub(0x30, 0x30, 0x30);
	pRenderContext->ClearBuffers(true, true);

	// Let it know our size
	int w, h;
	glfwGetWindowSize(m_pWindow, &w, &h);
	pRenderContext->Viewport(0, 0, w, h);

	// Begin ImGui
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = { (float)w, (float)h };
	io.DeltaTime = dt;
	ImGui::NewFrame();

	// ImGui UI Code 
	ImGui::ShowDemoWindow();
	if (ImGui::Begin("Cat")) { ImGui::Text("Dog"); } ImGui::End();
	if (ImGui::Begin("Dog")) { ImGui::Text("Cat"); } ImGui::End();

	// End ImGui
	ImGui::Render();
	ImGui_ImplSource_RenderDrawData(ImGui::GetDrawData());

	// End Frame
	g_pMaterialSystem->SwapBuffers();
	g_pMaterialSystem->EndFrame();
}
