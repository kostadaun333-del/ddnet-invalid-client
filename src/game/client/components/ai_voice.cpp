/* invalid — локальная озвучка через голосовой движок ОС. */
#include "ai_voice.h"

#include <base/log.h>

#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <objbase.h>
#include <sapi.h>
#include <windows.h>

static ISpVoice *s_pVoice = nullptr;
#else
#include <cstdio>
#include <cstdlib>
#endif

#ifdef _WIN32
static bool InitVoice()
{
	if(s_pVoice != nullptr)
		return true;
	const HRESULT Hr = CoInitialize(nullptr);
	if(FAILED(Hr) && Hr != RPC_E_CHANGED_MODE)
		return false;
	if(FAILED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, reinterpret_cast<void **>(&s_pVoice))))
	{
		CoUninitialize();
		return false;
	}
	return true;
}
#else
static std::string Quote(const std::string &Text)
{
	std::string Result = "'";
	for(char C : Text)
	{
		if(C == '\'')
			Result += "'\\''";
		else
			Result += C;
	}
	Result += "'";
	return Result;
}
#endif

void AiSpeakLocal(const char *pText, int Volume)
{
	if(pText == nullptr || pText[0] == '\0')
		return;
	const int ClampedVolume = Volume < 0 ? 0 : (Volume > 100 ? 100 : Volume);

#ifdef _WIN32
	if(!InitVoice())
	{
		dbg_msg("invalid/ai", "не удалось запустить локальный голос Windows (SAPI)");
		return;
	}
	const int WideLength = MultiByteToWideChar(CP_UTF8, 0, pText, -1, nullptr, 0);
	if(WideLength <= 0)
		return;
	std::wstring Wide(WideLength, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, pText, -1, &Wide[0], WideLength);
	s_pVoice->SetVolume((short)ClampedVolume);
	s_pVoice->Speak(Wide.c_str(), SPF_ASYNC, nullptr);
#else
	std::string Command;
	if(system("command -v espeak-ng >/dev/null 2>&1") == 0)
		Command = "espeak-ng -v ru -a " + std::to_string(ClampedVolume * 2) + " -- " + Quote(pText) + " >/dev/null 2>&1 &";
	else if(system("command -v espeak >/dev/null 2>&1") == 0)
		Command = "espeak -v ru -a " + std::to_string(ClampedVolume * 2) + " -- " + Quote(pText) + " >/dev/null 2>&1 &";
	else if(system("command -v spd-say >/dev/null 2>&1") == 0)
		Command = "spd-say " + Quote(pText) + " >/dev/null 2>&1 &";
	else
	{
		dbg_msg("invalid/ai", "локальный голос не найден (нужен espeak-ng, espeak или spd-say)");
		return;
	}
	if(system(Command.c_str()) != 0)
		dbg_msg("invalid/ai", "не удалось запустить локальную озвучку");
#endif
}

void AiShutdownVoice()
{
#ifdef _WIN32
	if(s_pVoice != nullptr)
	{
		s_pVoice->Release();
		s_pVoice = nullptr;
		CoUninitialize();
	}
#endif
}
