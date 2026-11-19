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
	if(DoButton_CheckBox(&s_EnabledId, "Включить functions invalid", g_Config.m_InvalidEnabled, &Row))
		g_Config.m_InvalidEnabled ^= 1;

	static char s_OverlayId;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_CheckBox(&s_OverlayId, "Оверлей справа сверху", g_Config.m_InvalidOverlay != 0, &Row))
		g_Config.m_InvalidOverlay = g_Config.m_InvalidOverlay ? 0 : 1;

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
		const char *apNames[] = {"Выключены", "Лисьи", "Кроличьи", "Бурые", "Дьявольские", "Неоновые"};
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
	Ui()->DoLabel(&Row, "Игроки на сервере", FontSize, TEXTALIGN_TL);

	static CButtonContainer s_AnalyzeButton;
	MainView.HSplitTop(26.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	if(DoButton_Menu(&s_AnalyzeButton, "Предварительный анализ (бин: invalid_analyze)", 0, &Row))
		GameClient()->m_Invalid.RunLocalAnalysis();

	const auto &vAssessments = GameClient()->m_Invalid.Assessments();
	static CButtonContainer s_aSpectateButtons[MAX_CLIENTS];
	static CButtonContainer s_aAssessButtons[MAX_CLIENTS];

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		const CGameClient::CClientData &ClientData = GameClient()->m_aClients[ClientId];
		if(!ClientData.m_Active || ClientData.m_Spec)
			continue;

		MainView.HSplitTop(26.0f, &Row, &MainView);
		s_ScrollRegion.AddRect(Row);

		CUIRect NameRect, SpectateRect, AssessRect;
		Row.VSplitRight(80.0f, &NameRect, &AssessRect);
		NameRect.VSplitRight(80.0f, &NameRect, &SpectateRect);

		char aName[64];
		str_format(aName, sizeof(aName), "%d: %s", ClientId, ClientData.m_aName);
		Ui()->DoLabel(&NameRect, aName, FontSize, TEXTALIGN_ML);

		if(DoButton_Menu(&s_aSpectateButtons[ClientId], "Спек", 0, &SpectateRect))
			GameClient()->m_Invalid.SpectatePlayer(ClientId);
		if(DoButton_Menu(&s_aAssessButtons[ClientId], "Анализ", 0, &AssessRect))
			GameClient()->m_Invalid.RunLocalAnalysis();
	}

	MainView.HSplitTop(14.0f, nullptr, &MainView);
	MainView.HSplitTop(22.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Демки", FontSize, TEXTALIGN_TL);
	MainView.HSplitTop(36.0f, &Row, &MainView);
	s_ScrollRegion.AddRect(Row);
	Ui()->DoLabel(&Row, "Загрузка демо и разбор по эвристикам — этап 3.", FontSize, TEXTALIGN_TL);

	if(!vAssessments.empty())
	{
		MainView.HSplitTop(14.0f, nullptr, &MainView);
		MainView.HSplitTop(22.0f, &Row, &MainView);
		s_ScrollRegion.AddRect(Row);
		Ui()->DoLabel(&Row, "Результаты (предварительно)", FontSize, TEXTALIGN_TL);
		for(const CInvalid::CAssessment &Assessment : vAssessments)
		{
			MainView.HSplitTop(18.0f, &Row, &MainView);
			s_ScrollRegion.AddRect(Row);
			char aLine[256];
			str_format(aLine, sizeof(aLine), "%s: %s — %s", Assessment.m_aName, CInvalid::VerdictLabel(Assessment.m_Verdict), Assessment.m_aReason);
			Ui()->DoLabel(&Row, aLine, FontSize - 2.0f, TEXTALIGN_TL);
		}
	}

	s_ScrollRegion.End();
}
