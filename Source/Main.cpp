#include <Windows.h>

#include <vector>
#include <string>
#include <ctime>
#include <filesystem>

namespace stdfs = std::filesystem;

typedef void(__thiscall* SfxCall_t)(
	void const*,
	char const*);

//Variables

static std::string_view constexpr MAIN_MODULE = "";

static SfxCall_t g_SfxCall = reinterpret_cast<SfxCall_t>(0xF680);
static uintptr_t g_destroyPlayer = 0x0020A5B0;

static std::vector<std::string> g_SfxList;

//Binder

__declspec(dllexport) void binder() {}

//Helpers

uintptr_t getModuleBase(std::string_view modName)
{
	return reinterpret_cast<uintptr_t>(
		GetModuleHandleA(modName.size() ? modName.data() : NULL));
}

inline bool makeWritable(
	uintptr_t const address,
	size_t const size)
{
	DWORD oldp;

	return VirtualProtect(
		reinterpret_cast<LPVOID>(address),
		size,
		PAGE_EXECUTE_READWRITE,
		&oldp);
}

inline bool doHooking(
	uintptr_t address,
	uintptr_t const callback,
	size_t size = 5,
	bool isCall = false)
{
	auto offset = callback - address - 5;
	auto p = reinterpret_cast<uint8_t*>(address);

	if (size < 5)
		size = 5;

	if (makeWritable(address, size))
	{
		*(p++) = isCall ? 0xE8 : 0xE9;

		for (auto i = 0u; i < sizeof(decltype(offset)); ++i)
			*(p++) = reinterpret_cast<uint8_t*>(&offset)[i];

		for (auto i = 5u; i < size; ++i)
			*(p++) = 0x90;

		return true;
	}

	return false;
}

//Callback

void __fastcall destroyPlayerCB(
	void const* ecx,
	void*,
	char const* fileName)
{
	if (g_SfxList.size())
	{
		auto size = g_SfxList.size();
		return g_SfxCall(ecx, ("Sfx/" + g_SfxList[rand() % size]).c_str());
	}

	return g_SfxCall(ecx, fileName);
}

//Entrypoint

DWORD WINAPI mainThread(LPVOID)
{
	srand(time(nullptr));

	//Get base address
	auto base = getModuleBase(MAIN_MODULE);

	//Calculate pointers
	g_SfxCall = reinterpret_cast<SfxCall_t>(
		reinterpret_cast<uintptr_t>(g_SfxCall) + base);

	g_destroyPlayer += base;

	//Get list of files
	std::error_code ec;

	for (auto const& entry :
		stdfs::directory_iterator("Resources/Sfx", ec))
	{
		auto const& path = entry.path();

		if (path.extension() == ".ogg")
			g_SfxList.push_back(path.filename().string());
	}

	//Place hook
	doHooking(
		g_destroyPlayer,
		reinterpret_cast<uintptr_t>(destroyPlayerCB),
		5,
		true);
}

BOOL WINAPI DllMain(
	HINSTANCE const dll,
	DWORD const reason,
	LPVOID const)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(dll);

		CreateThread(
			NULL,
			NULL,
			&mainThread,
			NULL,
			NULL,
			NULL);
	}

	return TRUE;
}