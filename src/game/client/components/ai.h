/* invalid — локальный AI-помощник (текст + голос). */
#ifndef GAME_CLIENT_COMPONENTS_AI_H
#define GAME_CLIENT_COMPONENTS_AI_H

#include <base/types.h>

#include <engine/console.h>

#include <game/client/component.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class CAi : public CComponent
{
public:
	CAi();
	~CAi() override;

	int Sizeof() const override { return sizeof(*this); }

	void OnInit() override;
	void OnShutdown() override;
	void OnConsoleInit() override;
	void OnRender() override;

	void Ask(const char *pText);
	void Roast();
	void Speak(const char *pText);
	// Вызывается из CChat::SendChat: реагирует на триггер в твоём сообщении.
	void OnLocalChat(const char *pLine);

	const char *Status() const { return m_aStatus; }
	bool IsBusy() const { return m_bBusy.load(); }

private:
	struct SRequest
	{
		std::string m_Question;
		std::string m_System;
		bool m_bTts = false;
		bool m_bTtsOnly = false;
	};

	struct SResult
	{
		std::string m_Text;
		std::vector<unsigned char> m_aAudio;
		char m_aError[256];
	};

	static void ConAsk(IConsole::IResult *pResult, void *pUserData);
	static void ConRoast(IConsole::IResult *pResult, void *pUserData);
	static void ConSpeak(IConsole::IResult *pResult, void *pUserData);
	static void ConMenu(IConsole::IResult *pResult, void *pUserData);

	void StartRequest(SRequest Request);
	void Worker(SRequest Request);
	void PlayApiAudio(const std::vector<unsigned char> &Data);
	void RenderHud();
	void SetStatus(const char *pText);
	static float SpeechDuration(const std::string &Text);

	std::mutex m_Mutex;
	std::vector<SResult> m_vPending;
	std::thread m_Thread;
	std::atomic<bool> m_bBusy{false};

	std::vector<std::pair<int, float>> m_vSamples;
	char m_aStatus[256];
	char m_aHudText[512];
	float m_HudUntil = 0.0f;
	float m_Time = 0.0f;
};

#endif
