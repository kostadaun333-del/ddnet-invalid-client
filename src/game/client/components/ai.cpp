/* invalid — локальный AI-помощник (текст + голос). */
#include "ai.h"

#include "ai_voice.h"

#include <base/log.h>
#include <base/str.h>
#include <base/vmath.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/shared/jsonwriter.h>
#include <engine/textrender.h>

#include <game/client/components/chat.h>
#include <game/client/components/menus.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <utility>

CAi::CAi()
{
	mem_zero(m_aStatus, sizeof(m_aStatus));
	mem_zero(m_aHudText, sizeof(m_aHudText));
	SetStatus("AI готов.");
}

CAi::~CAi()
{
	OnShutdown();
}

void CAi::SetStatus(const char *pText)
{
	str_copy(m_aStatus, pText, sizeof(m_aStatus));
}

void CAi::OnInit()
{
	SetStatus("AI готов. Бесплатный API уже настроен — можно жать «Роаст меня».");
}

void CAi::OnShutdown()
{
	if(m_Thread.joinable())
		m_Thread.join();
	for(auto &Sample : m_vSamples)
		Sound()->UnloadSample(Sample.first);
	m_vSamples.clear();
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		m_vPending.clear();
	}
	AiShutdownVoice();
}

void CAi::OnConsoleInit()
{
	Console()->Register("ai_ask", "r[text]", CFGFLAG_CLIENT, ConAsk, this, "Спросить AI (текст)");
	Console()->Register("ai_roast", "", CFGFLAG_CLIENT, ConRoast, this, "Роаст только для тебя (локально)");
	Console()->Register("ai_say", "r[text]", CFGFLAG_CLIENT, ConSpeak, this, "Озвучить текст локальным голосом");
	Console()->Register("ai_menu", "", CFGFLAG_CLIENT, ConMenu, this, "Открыть настройки AI");
}

void CAi::ConAsk(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CAi *>(pUserData)->Ask(pResult->GetString(0));
}

void CAi::ConRoast(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	static_cast<CAi *>(pUserData)->Roast();
}

void CAi::ConSpeak(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CAi *>(pUserData)->Speak(pResult->GetString(0));
}

void CAi::ConMenu(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	(void)pUserData;
	g_Config.m_UiSettingsPage = CMenus::SETTINGS_AI;
	GameClient()->m_Menus.SetActive(true);
}

void CAi::Ask(const char *pText)
{
	if(!g_Config.m_AiEnabled)
	{
		SetStatus("AI выключен.");
		return;
	}
	if(pText == nullptr || pText[0] == '\0')
	{
		SetStatus("Введи сообщение для AI.");
		return;
	}

	SRequest Request;
	Request.m_Question = pText;
	Request.m_System = g_Config.m_AiSystem;
	StartRequest(std::move(Request));
}

void CAi::Roast()
{
	if(!g_Config.m_AiEnabled)
	{
		SetStatus("AI выключен.");
		return;
	}

	char aName[64] = "друг";
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId >= 0 && LocalId < MAX_CLIENTS && GameClient()->m_aClients[LocalId].m_aName[0] != '\0')
		str_copy(aName, GameClient()->m_aClients[LocalId].m_aName, sizeof(aName));

	SRequest Request;
	Request.m_System =
		"Ты комедийный роаст-бот в DDNet. Игрок сам нажал кнопку и хочет, чтобы над ним подшутили. "
		"Отвечай по-русски, коротко (до 80 слов), дерзко и смешно: подшути над самим игроком, его ником, "
		"его родителями и семьёй, его игрой и привычками. Это дружеская шутка только для одного человека. "
		"Не используй угрозы, призывы к ненависти, дискриминацию и реальные персональные данные.";
	Request.m_Question = std::string("Меня зовут ") + aName + ". Роастни меня, AI.";
	StartRequest(std::move(Request));
}

void CAi::Speak(const char *pText)
{
	if(!g_Config.m_AiEnabled)
	{
		SetStatus("AI выключен.");
		return;
	}
	if(pText == nullptr || pText[0] == '\0')
	{
		SetStatus("Введи текст для озвучки.");
		return;
	}

	AiSpeakLocal(pText, g_Config.m_AiVolume);
	str_copy(m_aHudText, pText, sizeof(m_aHudText));
	m_HudUntil = m_Time + SpeechDuration(m_aHudText);
	SetStatus("Озвучиваю локально.");
}

void CAi::StartRequest(SRequest Request)
{
	if(m_bBusy.load())
	{
		SetStatus("AI ещё думает, подожди...");
		return;
	}
	if(m_Thread.joinable())
		m_Thread.join();
	m_bBusy = true;
	SetStatus("AI думает...");
	m_Thread = std::thread(&CAi::Worker, this, std::move(Request));
}

void CAi::Worker(SRequest Request)
{
	SResult Result;
	Result.m_aError[0] = '\0';
	if(Request.m_bTtsOnly)
		Result.m_Text = Request.m_Question;

	// 1. Чат-запрос к OpenAI-совместимому API.
	if(!Request.m_bTtsOnly)
	{
		CJsonStringWriter Writer;
		Writer.BeginObject();
		Writer.WriteAttribute("model");
		Writer.WriteStrValue(g_Config.m_AiModel);
		Writer.WriteAttribute("messages");
		Writer.BeginArray();
		Writer.BeginObject();
		Writer.WriteAttribute("role");
		Writer.WriteStrValue("system");
		Writer.WriteAttribute("content");
		Writer.WriteStrValue(Request.m_System.c_str());
		Writer.EndObject();
		Writer.BeginObject();
		Writer.WriteAttribute("role");
		Writer.WriteStrValue("user");
		Writer.WriteAttribute("content");
		Writer.WriteStrValue(Request.m_Question.c_str());
		Writer.EndObject();
		Writer.EndArray();
		Writer.WriteAttribute("max_tokens");
		Writer.WriteIntValue(350);
		Writer.EndObject();
		const std::string Body = Writer.GetOutputString();

		std::shared_ptr<IHttpRequest> pReq = HttpPostJson(g_Config.m_AiEndpoint, Body.c_str());
		pReq->WriteToMemory();
		pReq->FailOnErrorStatus(false);
		if(g_Config.m_AiKey[0] != '\0')
		{
			char aAuth[512];
			str_format(aAuth, sizeof(aAuth), "Bearer %s", g_Config.m_AiKey);
			pReq->HeaderString("Authorization", aAuth);
		}

		GameClient()->Http()->Run(pReq);
		pReq->Wait();

		if(pReq->State() != EHttpState::DONE || pReq->StatusCode() < 200 || pReq->StatusCode() >= 300)
		{
			str_format(Result.m_aError, sizeof(Result.m_aError), "Ошибка API: HTTP %d. Проверь интернет и лимиты.", pReq->StatusCode());
			m_bBusy = false;
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_vPending.push_back(std::move(Result));
			return;
		}

		json_value *pJson = pReq->ResultJson();
		if(pJson != nullptr)
		{
			const json_value &Content = (*pJson)["choices"][0]["message"]["content"];
			if(Content.type == json_string)
				Result.m_Text = json_string_get(&Content);
			json_value_free(pJson);
		}
		if(Result.m_Text.empty())
			str_copy(Result.m_aError, "API вернул пустой ответ. Проверь модель и ключ.", sizeof(Result.m_aError));
	}

	// 2. Необязательный API-TTS. По умолчанию голос даёт локальный движок ОС.
	if(Result.m_aError[0] == '\0' && g_Config.m_AiVoiceApi && g_Config.m_AiTtsEndpoint[0] != '\0')
	{
		CJsonStringWriter TtsWriter;
		TtsWriter.BeginObject();
		TtsWriter.WriteAttribute("model");
		TtsWriter.WriteStrValue(g_Config.m_AiTtsModel);
		TtsWriter.WriteAttribute("voice");
		TtsWriter.WriteStrValue(g_Config.m_AiTtsVoice);
		TtsWriter.WriteAttribute("input");
		TtsWriter.WriteStrValue(Result.m_Text.c_str());
		TtsWriter.WriteAttribute("response_format");
		TtsWriter.WriteStrValue("opus");
		TtsWriter.EndObject();
		const std::string TtsBody = TtsWriter.GetOutputString();

		std::shared_ptr<IHttpRequest> pTts = HttpPostJson(g_Config.m_AiTtsEndpoint, TtsBody.c_str());
		pTts->WriteToMemory();
		pTts->FailOnErrorStatus(false);
		if(g_Config.m_AiKey[0] != '\0')
		{
			char aAuth[512];
			str_format(aAuth, sizeof(aAuth), "Bearer %s", g_Config.m_AiKey);
			pTts->HeaderString("Authorization", aAuth);
		}
		GameClient()->Http()->Run(pTts);
		pTts->Wait();

		if(pTts->State() == EHttpState::DONE && pTts->StatusCode() >= 200 && pTts->StatusCode() < 300)
		{
			unsigned char *pData = nullptr;
			size_t DataLength = 0;
			pTts->Result(&pData, &DataLength);
			if(pData != nullptr && DataLength > 0)
				Result.m_aAudio.assign(pData, pData + DataLength);
		}
		else
		{
			dbg_msg("invalid/ai", "API-TTS недоступен (HTTP %d), используется локальный голос", pTts->StatusCode());
		}
	}

	m_bBusy = false;
	std::lock_guard<std::mutex> Lock(m_Mutex);
	m_vPending.push_back(std::move(Result));
}

void CAi::PlayApiAudio(const std::vector<unsigned char> &Data)
{
	const int SampleId = Sound()->LoadOpusFromMem(Data.data(), (unsigned)Data.size(), true, "ai_tts");
	if(SampleId == -1)
	{
		dbg_msg("invalid/ai", "не удалось декодировать opus из API-TTS");
		return;
	}
	Sound()->Play(CSounds::CHN_GLOBAL, SampleId, 0, (float)g_Config.m_AiVolume / 100.0f);
	m_vSamples.emplace_back(SampleId, m_Time);
}

float CAi::SpeechDuration(const std::string &Text)
{
	const float Duration = 4.0f + (float)Text.size() * 0.075f;
	return std::min(30.0f, std::max(5.0f, Duration));
}

void CAi::RenderHud()
{
	if(!g_Config.m_AiEnabled || m_HudUntil <= m_Time || m_aHudText[0] == '\0')
		return;

	Ui()->MapScreen();
	const CUIRect *pScreen = Ui()->Screen();
	if(pScreen == nullptr)
		return;
	const float FontSize = 11.0f;
	const float Width = std::min(460.0f, pScreen->w - 20.0f);
	const float X = pScreen->w * 0.5f - Width * 0.5f;
	const float Y = 12.0f;
	const CUIRect Box = CUIRect{X, Y, Width, 52.0f};
	Box.Draw(ColorRGBA(0.05f, 0.06f, 0.10f, 0.82f), IGraphics::CORNER_ALL, 6.0f);

	TextRender()->Text(X + 8.0f, Y + 4.0f, 10.0f, "AI ГОВОРИТ");

	// Анимированный эквалайзер, пока идёт озвучка.
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(int i = 0; i < 12; ++i)
	{
		const float H = 3.0f + 11.0f * (0.5f + 0.5f * std::sin(m_Time * 9.0f + i * 0.7f));
		Graphics()->SetColor(0.35f, 0.75f + i * 0.02f, 1.0f, 0.9f);
		const IGraphics::CQuadItem Bar(X + Width - 88.0f + i * 6.5f, Y + 6.0f + (12.0f - H), 3.0f, H);
		Graphics()->QuadsDraw(&Bar, 1);
	}
	Graphics()->QuadsEnd();

	char aLine[300];
	str_copy(aLine, m_aHudText, sizeof(aLine));
	while(TextRender()->TextWidth(FontSize, aLine) > Width - 16.0f && std::strlen(aLine) > 4)
		aLine[std::strlen(aLine) - 1] = '\0';
	TextRender()->Text(X + 8.0f, Y + 22.0f, FontSize, aLine);
}

void CAi::OnRender()
{
	m_Time = Client()->GlobalTime();

	for(size_t i = 0; i < m_vSamples.size();)
	{
		if(m_Time - m_vSamples[i].second > 30.0f)
		{
			Sound()->UnloadSample(m_vSamples[i].first);
			m_vSamples.erase(m_vSamples.begin() + i);
		}
		else
			++i;
	}

	std::vector<SResult> vResults;
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		vResults.swap(m_vPending);
	}

	for(SResult &Result : vResults)
	{
		if(Result.m_aError[0] != '\0')
		{
			SetStatus(Result.m_aError);
			char aMsg[512];
			str_format(aMsg, sizeof(aMsg), "AI: %s", Result.m_aError);
			GameClient()->m_Chat.AddLine(-1, 0, aMsg);
			continue;
		}

		SetStatus("AI ответил.");
		str_copy(m_aHudText, Result.m_Text.c_str(), sizeof(m_aHudText));
		m_HudUntil = m_Time + SpeechDuration(m_aHudText);

		char aMsg[600];
		str_format(aMsg, sizeof(aMsg), "AI: %s", Result.m_Text.c_str());
		std::replace(aMsg, aMsg + std::strlen(aMsg), '\n', ' ');
		// По умолчанию ответ остаётся только у тебя — в чат уходит,
		// только если включён отдельный пункт в настройках.
		if(g_Config.m_AiSendToChat && Client()->State() == IClient::STATE_ONLINE)
			GameClient()->m_Chat.SendChat(0, aMsg);
		else
			GameClient()->m_Chat.AddLine(-1, 0, aMsg);

		if(g_Config.m_AiVoice)
		{
			if(g_Config.m_AiVoiceApi && !Result.m_aAudio.empty())
				PlayApiAudio(Result.m_aAudio);
			else
				AiSpeakLocal(Result.m_Text.c_str(), g_Config.m_AiVolume);
		}
	}

	RenderHud();
}
