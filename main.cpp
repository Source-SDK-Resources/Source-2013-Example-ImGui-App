#include "app.h"
#include "tier0/icommandline.h"
#include "appframework/appframework.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "vphysics_interface.h"
#include "Datacache/imdlcache.h"
#include "datacache/idatacache.h"
#include "filesystem_init.h"
#include "tier1/tier1.h"
#include "tier2/tier2.h"

#include "memdbgoff.h"

IFileSystem* g_pFileSystem;
IStudioRender* g_pStudioRender;
IMDLCache* g_pMDLCache;

class CSteamAppLoader : public CSteamAppSystemGroup
{
public:
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown() {}
	virtual void Destroy();

};

int main (int argc, char **argv)
{
	CommandLine()->CreateCmdLine( argc, argv );

	CSteamAppLoader smaugsteamapp;
	CSteamApplication steamapp( &smaugsteamapp );
	return steamapp.Run();
}

// Prep ourselves for app system loading
bool CSteamAppLoader::PreInit()
{
	// Set up the filesystem

	// Make sure we always at least have something?
	// Otherwise the next step will close the program
	if (!CommandLine()->FindParm("-game"))
		CommandLine()->AppendParm("-game", "../hl2");

	// Find the route to our gameinfo file
	CFSSteamSetupInfo steamInfo;
	steamInfo.m_pDirectoryName = NULL;
	steamInfo.m_bOnlyUseDirectoryName = false;
	steamInfo.m_bToolsMode = true;
	steamInfo.m_bSetSteamDLLPath = true;
	steamInfo.m_bSteam = g_pFileSystem->IsSteam();
	if (FileSystem_SetupSteamEnvironment(steamInfo) != FS_OK)
		return false;

	// Mount the game 
	CFSMountContentInfo fsInfo;
	fsInfo.m_pFileSystem = g_pFileSystem;
	fsInfo.m_bToolsMode = true;
	fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;
	if (FileSystem_MountContent(fsInfo) != FS_OK)
		return false;

	// Finally, load the search paths for the "GAME" path.
	CFSSearchPathsInit searchPathsInit;
	searchPathsInit.m_pDirectoryName = steamInfo.m_GameInfoPath;
	searchPathsInit.m_pFileSystem = fsInfo.m_pFileSystem;
	if (FileSystem_LoadSearchPaths(searchPathsInit) != FS_OK)
		return false;

	// Add platform to our search path
	char platform[MAX_PATH];
	Q_strncpy(platform, steamInfo.m_GameInfoPath, MAX_PATH);
	Q_StripTrailingSlash(platform);
	Q_strncat(platform, "/../platform", MAX_PATH, MAX_PATH);
	fsInfo.m_pFileSystem->AddSearchPath(platform, "PLATFORM");


	MathLib_Init();

	g_pMaterialSystem->SetAdapter(0, 0);

	return true;
}

// Load up all our app systems
bool CSteamAppLoader::Create()
{
	AppSystemInfo_t appSystems[] =
	{
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION   },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION }, // Annoyingly, we need vphyiscs as well :P
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	if (!AddSystems(appSystems))
		return false;

	CreateInterfaceFn factory = GetFactory();
	ConnectTier1Libraries(&factory, 1);
	ConnectTier2Libraries(&factory, 1);

	g_pFileSystem      = (IFileSystem*)FindSystem(FILESYSTEM_INTERFACE_VERSION);
	g_pMaterialSystem  = (IMaterialSystem*)FindSystem(MATERIAL_SYSTEM_INTERFACE_VERSION);
	g_pStudioRender	   = (IStudioRender*)FindSystem(STUDIO_RENDER_INTERFACE_VERSION);
	g_pMDLCache		   = (IMDLCache*)FindSystem(MDLCACHE_INTERFACE_VERSION);

	if (!g_pFileSystem || !g_pMaterialSystem || !g_pStudioRender || !g_pMDLCache)
	{
		Error("Unable to load required library interface!\n");
		return false;
	}

	g_pMaterialSystem->SetShaderAPI("shaderapidx9");
	g_pMaterialSystem->Connect(factory);

	// Must be done after material system is connected up!
	g_pMaterialSystemHardwareConfig = (IMaterialSystemHardwareConfig*)FindSystem(MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION);

	return true;
}

// Disconnect our app systems
void CSteamAppLoader::Destroy()
{
	DisconnectTier1Libraries();
	DisconnectTier2Libraries();

	g_pFileSystem = NULL;
	g_pMaterialSystem = NULL;
}

int CSteamAppLoader::Main()
{
	g_pMaterialSystem->ModInit();

	// Starts up the app and runs the main loop
	CImGuiSourceApp* app = new CImGuiSourceApp;
	app->Init();
	app->Destroy();
	delete app;

	g_pMaterialSystem->ModShutdown();

	return 0;
}

