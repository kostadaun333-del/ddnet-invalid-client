/* invalid — вкладка настроек AI-помощника. */
#include "menus.h"

#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/ai.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>

void CMenus::RenderSettingsAi(CUIRect MainView)
{
	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 40.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);

	static bool s_Initialized = false;
	static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_Endpoint;
	static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_Key;
	static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_Model;
	static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_System;
	static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_TtsEndpoint;
	static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_TtsModel;
	static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_TtsVoice;
	static CLineInputBuffered<64> s_Trigger;
	static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_Question;

	if(!s_Initialized)
	{
		s_Endpoint.Set(g_Config.m_AiEndpoint);
		s_Key.Set(g_Config.m_AiKey);
		s_Model.Set(g_Config.m_AiModel);
		s_System.Set(g_Config.m_AiSystem);
		s_TtsEndpoint.Set(g_Config.m_AiTtsEndpoint);
		s_TtsModel.Set(g_Config.m_AiTtsModel);
		s_TtsVoice.Set(g_Config.m_AiTtsVoice);
		s_Trigger.Set(g_Config.m_AiTrigger);
		s_Initialized = true;
	}

	str_copy(g_Config.m_AiEndpoint, s_Endpoint.GetString(), sizeof(g_Config.m_AiEndpoint));
	str_copy(g_Config.m_AiKey, s_Key.GetString(), sizeof(g_Config.m_AiKey));
	str_copy(g_Config.m_AiModel, s_Model.GetString(), sizeof(g_Config.m_AiModel));
	str_copy(g_Config.m_AiSystem, s_System.GetString(), sizeof(g_Config.m_AiSystem));
	str_copy(g_Config.m_AiTtsEndpoint, s_TtsEndpoint.GetString(), sizeof(g_Config.m_AiTtsEndpoint));
	str_copy(g_Config.m_AiTtsModel, s_TtsModel.GetString(), sizeof(g_Config.m_AiTtsModel));
	str_copy(g_Config.m_AiTtsVoice, s_TtsVoice.GetString(), sizeof(g_Config.m_AiTtsVoice));
	str_copy(g_Config.m_AiTrigger, s_Trigger.GetString(), sizeof(g_Config.m_AiTrigger));

	const float FontSize = 13.0f;
	CUIRect Row;

	MainView.HSplitTop(70.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "AI-помощник", 26.0f, TEXTALIGN_TL);
	Row.HSplitTop(30.0f, nullptr, &Row);
	Ui()->DoLabel(&Row, "Голосовая ИИ. Работает через бесплатный OpenAI-совместимый API.", FontSize, TEXTALIGN_TL);
	Row.HSplitTop(18.0f, nullptr, &Row);
	Ui()->DoLabel(&Row, "Голос по умолчанию локальный (Windows SAPI / espeak), поэтому ключ для озвучки не нужен.", FontSize, TEXTALIGN_TL);

	MainView.HSplitTop(8.0f, nullptr, &MainView);

	static char s_EnabledId;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_CheckBox(&s_EnabledId, "Включить AI", g_Config.m_AiEnabled, &Row))
		g_Config.m_AiEnabled ^= 1;

	static char s_VoiceId;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_CheckBox(&s_VoiceId, "Отвечать голосом", g_Config.m_AiVoice, &Row))
		g_Config.m_AiVoice ^= 1;

	static char s_ApiVoiceId;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_CheckBox(&s_ApiVoiceId, "Использовать API-TTS вместо локального голоса", g_Config.m_AiVoiceApi, &Row))
		g_Config.m_AiVoiceApi ^= 1;

	static char s_ChatId;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_CheckBox(&s_ChatId, "Отправлять ответ в общий чат (по умолчанию выключено)", g_Config.m_AiSendToChat, &Row))
		g_Config.m_AiSendToChat ^= 1;

	MainView.HSplitTop(8.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	char aVolume[64];
	str_format(aVolume, sizeof(aVolume), "Громкость голоса: %d%%", g_Config.m_AiVolume);
	Ui()->DoLabel(&Row, aVolume, FontSize - 1.0f, TEXTALIGN_TL);
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	static char s_VolumeId;
	g_Config.m_AiVolume = (int)(Ui()->DoScrollbarH(&s_VolumeId, &Row, (float)g_Config.m_AiVolume / 100.0f) * 100.0f);

	MainView.HSplitTop(14.0f, nullptr, &MainView);

	auto AddEdit = [&](const char *pLabel, CLineInputBuffered<IO_MAX_PATH_LENGTH> &Input) {
		MainView.HSplitTop(22.0f, &Row, &MainView);
		s_ScrollRegion.AddRect(Row);
		Ui()->DoLabel(&Row, pLabel, FontSize - 2.0f, TEXTALIGN_ML);
		MainView.HSplitTop(24.0f, &Row, &MainView);
		s_ScrollRegion.AddRect(Row);
		Ui()->DoEditBox(&Input, &Row, 12.0f);
	};

	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Когда ИИ отвечает", FontSize, TEXTALIGN_TL);
	const char *apTriggerModes[] = {"Только кнопки", "По слову-триггеру", "На любое сообщение"};
	static CButtonContainer s_aTriggerButtons[3];
	for(int Mode = 0; Mode < 3; ++Mode)
	{
		MainView.HSplitTop(26.0f, &Row, &MainView);
		s_ScrollRegion.AddRect(Row);
		if(DoButton_Menu(&s_aTriggerButtons[Mode], apTriggerModes[Mode], g_Config.m_AiTriggerMode == Mode, &Row))
			g_Config.m_AiTriggerMode = Mode;
	}
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Слово-триггер (по умолчанию «ии»):", FontSize - 2.0f, TEXTALIGN_ML);
	MainView.HSplitTop(24.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoEditBox(&s_Trigger, &Row, 12.0f);

	MainView.HSplitTop(14.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Подключение (по умолчанию — бесплатный Pollinations без ключа)", FontSize, TEXTALIGN_TL);
	AddEdit("URL чата:", s_Endpoint);
	AddEdit("API-ключ (можно пустой):", s_Key);
	AddEdit("Модель чата:", s_Model);
	AddEdit("System prompt (стиль ИИ):", s_System);

	MainView.HSplitTop(14.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "API-TTS (нужен только если включён выше)", FontSize, TEXTALIGN_TL);
	AddEdit("URL TTS:", s_TtsEndpoint);
	AddEdit("Модель TTS:", s_TtsModel);
	AddEdit("Голос:", s_TtsVoice);

	MainView.HSplitTop(14.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Сообщение для AI", FontSize, TEXTALIGN_TL);
	AddEdit("Текст:", s_Question);

	static CButtonContainer s_AskButton;
	MainView.HSplitTop(28.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_Menu(&s_AskButton, "Спросить AI (ответ голосом)", 0, &Row))
		GameClient()->m_Ai.Ask(s_Question.GetString());

	static CButtonContainer s_RoastButton;
	MainView.HSplitTop(28.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_Menu(&s_RoastButton, "Роаст меня (только локально, голосом)", 0, &Row))
		GameClient()->m_Ai.Roast();

	static CButtonContainer s_SpeakButton;
	MainView.HSplitTop(28.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_Menu(&s_SpeakButton, "Озвучить введённый текст", 0, &Row))
		GameClient()->m_Ai.Speak(s_Question.GetString());

	MainView.HSplitTop(24.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	char aStatus[512];
	str_format(aStatus, sizeof(aStatus), "Статус: %s%s", GameClient()->m_Ai.Status(), GameClient()->m_Ai.IsBusy() ? " (работаю...)" : "");
	Ui()->DoLabel(&Row, aStatus, FontSize - 2.0f, TEXTALIGN_TL);

	s_ScrollRegion.End();
}
