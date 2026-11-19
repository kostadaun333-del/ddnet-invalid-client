/* invalid — вкладка настроек. */
#include "menus.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/invalid.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>

void CMenus::RenderSettingsInvalid(CUIRect MainView)
{
	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 40.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);

	const float FontSize = 13.0f;
	CUIRect Row, Left, Right;

	MainView.HSplitTop(70.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "invalid", 26.0f, TEXTALIGN_TL);
	Row.HSplitTop(30.0f, nullptr, &Row);
	Ui()->DoLabel(&Row, "Эвристический анализ игроков. Оценка не является доказательством и может ошибаться.", FontSize, TEXTALIGN_TL);
	Row.HSplitTop(18.0f, nullptr, &Row);
	Ui()->DoLabel(&Row, "Не используйте результаты для публичных обвинений — сначала проверяйте демо вручную.", FontSize, TEXTALIGN_TL);

	MainView.HSplitTop(8.0f, nullptr, &MainView);

	static char s_EnabledId;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_CheckBox(&s_EnabledId, "Включить invalid", g_Config.m_InvalidEnabled, &Row))
		g_Config.m_InvalidEnabled ^= 1;

	static char s_OverlayId;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_CheckBox(&s_OverlayId, "Оверлей справа сверху", g_Config.m_InvalidOverlay != 0, &Row))
		g_Config.m_InvalidOverlay = g_Config.m_InvalidOverlay ? 0 : 2;

	static char s_ReasonsId;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_CheckBox(&s_ReasonsId, "Показывать причины оценки в оверлее", g_Config.m_InvalidShowReasons, &Row))
		g_Config.m_InvalidShowReasons ^= 1;

	MainView.HSplitTop(14.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Питомец (летает за тобой)", FontSize, TEXTALIGN_TL);
	for(int Mode = 0; Mode <= 3; ++Mode)
	{
		MainView.HSplitTop(24.0f, &Row, &MainView);
		s_ScrollRegion.AddRect(Row);
		Row.VSplitMid(&Left, &Right);
		static CButtonContainer s_aPetButtons[4];
		const char *apNames[] = {"Выключен", "Сфера", "Звезда", "Радуга"};
		if(DoButton_Menu(&s_aPetButtons[Mode], apNames[Mode], g_Config.m_InvalidPet == Mode, &Right))
			g_Config.m_InvalidPet = Mode;
	}

	MainView.HSplitTop(10.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Картинка питомца (.png, полный путь)", FontSize, TEXTALIGN_TL);
	MainView.HSplitTop(24.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoEditBox(&GameClient()->m_Invalid.PetImageInput(), &Row, 12.0f);

	MainView.HSplitTop(14.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Ушки", FontSize, TEXTALIGN_TL);
	for(int Mode = 0; Mode <= 5; ++Mode)
	{
		MainView.HSplitTop(24.0f, &Row, &MainView);
		s_ScrollRegion.AddRect(Row);
		Row.VSplitMid(&Left, &Right);
		static CButtonContainer s_aEarsButtons[6];
		const char *apNames[] = {"Выключены", "Лисы", "Кролики", "Бурые", "Дьявольские", "Неоновые"};
		if(DoButton_Menu(&s_aEarsButtons[Mode], apNames[Mode], g_Config.m_InvalidEars == Mode, &Right))
			g_Config.m_InvalidEars = Mode;
	}

	MainView.HSplitTop(14.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Пранк (только локально, другие игроки его не видят)", FontSize, TEXTALIGN_TL);
	static CButtonContainer s_PrankButton;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_Menu(&s_PrankButton, "Дождевое облачко (бин: invalid_prank)", 0, &Row))
		GameClient()->m_Invalid.TriggerPrank();

	MainView.HSplitTop(14.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Фон меню", FontSize, TEXTALIGN_TL);
	static CButtonContainer s_RainButton;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	const char *apRainNames[] = {"Дождь выключен", "Дождь в меню", "Дождь всегда"};
	if(DoButton_Menu(&s_RainButton, apRainNames[g_Config.m_InvalidRain], 0, &Row))
		g_Config.m_InvalidRain = (g_Config.m_InvalidRain + 1) % 3;

	static char s_LightningId;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_CheckBox(&s_LightningId, "Молнии", g_Config.m_InvalidLightning, &Row))
		g_Config.m_InvalidLightning ^= 1;

	MainView.HSplitTop(14.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Анализ демки", FontSize, TEXTALIGN_TL);

	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Путь к .demo (можно полный путь с диском):", FontSize - 2.0f, TEXTALIGN_TL);
	MainView.HSplitTop(24.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoEditBox(&GameClient()->m_Invalid.DemoPathInput(), &Row, 12.0f);

	static CButtonContainer s_LoadButton;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_Menu(&s_LoadButton, "Загрузить демо и проанализировать", 0, &Row))
		GameClient()->m_Invalid.LoadDemoAndAnalyze(GameClient()->m_Invalid.DemoPathInput().GetString());

	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	char aStatus[300];
	str_format(aStatus, sizeof(aStatus), "Статус: %s", GameClient()->m_Invalid.Status());
	Ui()->DoLabel(&Row, aStatus, FontSize - 2.0f, TEXTALIGN_TL);

	MainView.HSplitTop(14.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Игроки на сервере", FontSize, TEXTALIGN_TL);

	static CButtonContainer s_AnalyzeButton;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_Menu(&s_AnalyzeButton, "Предварительный список игроков", 0, &Row))
		GameClient()->m_Invalid.RunLocalAnalysis();

	static CButtonContainer s_aSpectateButtons[MAX_CLIENTS];
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		const CGameClient::CClientData &ClientData = GameClient()->m_aClients[ClientId];
		if(!ClientData.m_Active || ClientData.m_Spec)
			continue;

		MainView.HSplitTop(26.0f, &Row, &MainView);
		s_ScrollRegion.AddRect(Row);

		CUIRect NameRect, SpectateRect;
		Row.VSplitRight(90.0f, &NameRect, &SpectateRect);

		char aName[64];
		str_format(aName, sizeof(aName), "%d: %s", ClientId, ClientData.m_aName);
		Ui()->DoLabel(&NameRect, aName, FontSize, TEXTALIGN_ML);

		if(DoButton_Menu(&s_aSpectateButtons[ClientId], "Спек", 0, &SpectateRect))
			GameClient()->m_Invalid.SpectatePlayer(ClientId);
	}

	const auto &vAssessments = GameClient()->m_Invalid.Assessments();
	if(!vAssessments.empty())
	{
		MainView.HSplitTop(14.0f, nullptr, &MainView);
		MainView.HSplitTop(22.0f, &Row, &MainView);
		s_ScrollRegion.AddRect(Row);
		Ui()->DoLabel(&Row, "Результаты анализа демки", FontSize, TEXTALIGN_TL);
		for(const CInvalid::CAssessment &A : vAssessments)
		{
			MainView.HSplitTop(32.0f, &Row, &MainView);
			s_ScrollRegion.AddRect(Row);
			char aLine[300];
			str_format(aLine, sizeof(aLine), "%s: %s — %d%%", A.m_aName, CInvalid::VerdictLabel(A.m_Verdict), (int)(A.m_Score * 100.0f));
			Ui()->DoLabel(&Row, aLine, FontSize - 1.0f, TEXTALIGN_TL);
			CUIRect ReasonRect = Row;
			ReasonRect.y += 15.0f;
			Ui()->DoLabel(&ReasonRect, A.m_aReason, FontSize - 3.0f, TEXTALIGN_TL);
		}
	}

	s_ScrollRegion.End();
}
