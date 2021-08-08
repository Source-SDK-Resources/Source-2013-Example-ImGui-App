#pragma once

struct GLFWwindow;
class ITexture;

// I would just merge this into CSteamAppLoader, but it makes such a mess
class CImGuiSourceApp
{
public:
	void Init();
	void Destroy();
private:
	void Draw();

	GLFWwindow* m_pWindow;
	ITexture *m_pWhiteTexture;

	float m_lastFrameTime;
};
