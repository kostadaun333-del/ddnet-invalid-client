/* invalid — клиентский компонент для DDNet. */
#include "invalid.h"

#include <base/color.h>
#include <base/log.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/components/spectator.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <cmath>
#include <cstring>

CInvalid::CInvalid()
{
	OnReset();
}

void CInvalid::OnReset()
{
	m_vAssessments.clear();
	m_PrankUntil = 0.0f;
	m_Time = 0.0f;
}

void CInvalid::OnConsoleInit()
{
	Console()->Register("invalid_menu", "", CFGFLAG_CLIENT, ConInvalidMenu, this, "Открыть меню invalid");
	Console()->Register("invalid_prank", "", CFGFLAG_CLIENT, ConInvalidPrank, this, "Локальный пранк: дождевое облачко над головой");
	Console()->Register("invalid_analyze", "", CFGFLAG_CLIENT, ConInvalidAnalyze, this, "Предварительный анализ игроков в текущем снимке");
	Console()->Register("invalid_spectate", "i[client-id]", CFGFLAG_CLIENT, ConInvalidSpectate, this, "Спектатор за выбранным игроком");
	Console()->Register("invalid_pet", "i[mode]", CFGFLAG_CLIENT, ConInvalidPet, this, "Питомец: 0 — выкл, 1 — сфера, 2 — звезда, 3 — радуга");
	Console()->Register("invalid_ears", "i[mode]", CFGFLAG_CLIENT, ConInvalidEars, this, "Ушки: 0 — выкл, 1..5 — разные варианты");
}

void CInvalid::ConInvalidMenu(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	static_cast<CInvalid *>(pUserData)->OpenMenu();
}

void CInvalid::ConInvalidPrank(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	static_cast<CInvalid *>(pUserData)->TriggerPrank();
}

void CInvalid::ConInvalidAnalyze(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	static_cast<CInvalid *>(pUserData)->RunLocalAnalysis();
}

void CInvalid::ConInvalidSpectate(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CInvalid *>(pUserData)->SpectatePlayer(pResult->GetInteger(0));
}

void CInvalid::ConInvalidPet(IConsole::IResult *pResult, void *pUserData)
{
	(void)pUserData;
	g_Config.m_InvalidPet = pResult->GetInteger(0);
}

void CInvalid::ConInvalidEars(IConsole::IResult *pResult, void *pUserData)
{
	(void)pUserData;
	g_Config.m_InvalidEars = pResult->GetInteger(0);
}

void CInvalid::OpenMenu()
{
	g_Config.m_UiSettingsPage = CMenus::SETTINGS_INVALID;
	GameClient()->m_Menus.SetActive(true);
}

void CInvalid::RunLocalAnalysis()
{
	m_vAssessments.clear();

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		const CGameClient::CClientData &ClientData = GameClient()->m_aClients[ClientId];
		if(!ClientData.m_Active || ClientData.m_Spec)
			continue;

		CAssessment Assessment;
		Assessment.m_ClientId = ClientId;
		str_copy(Assessment.m_aName, ClientData.m_aName, sizeof(Assessment.m_aName));
		Assessment.m_Verdict = EVerdict::NEUTRAL;
		Assessment.m_Score = 0.0f;
		Assessment.m_bHasData = false;
		str_copy(Assessment.m_aReason, "Недостаточно данных. Загрузите демо для полноценного анализа.", sizeof(Assessment.m_aReason));
		m_vAssessments.push_back(Assessment);
	}
}

void CInvalid::SpectatePlayer(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	GameClient()->m_Spectator.Spectate(ClientId);
}

void CInvalid::TriggerPrank()
{
	m_PrankUntil = m_Time + 4.0f;
}

int CInvalid::LocalClientId() const
{
	return GameClient()->m_aLocalIds[g_Config.m_ClDummy];
}

void CInvalid::OnRender()
{
	m_Time = Client()->GlobalTime();

	if(!g_Config.m_InvalidEnabled)
		return;

	const int State = Client()->State();
	if(State != IClient::STATE_ONLINE && State != IClient::STATE_DEMOPLAYBACK)
		return;

	// Косметика рисуется в мировых координатах, пока не вызван Ui()->MapScreen().
	if(g_Config.m_InvalidPet || g_Config.m_InvalidEars)
		RenderCosmetics();
	if(m_Time < m_PrankUntil)
		RenderPrank();

	if(g_Config.m_InvalidOverlay != 0)
		RenderOverlay();
}

void CInvalid::RenderCosmetics()
{
	const int LocalId = LocalClientId();
	if(LocalId < 0 || LocalId >= MAX_CLIENTS)
		return;

	const CGameClient::CClientData &ClientData = GameClient()->m_aClients[LocalId];
	if(!ClientData.m_Active)
		return;

	if(g_Config.m_InvalidPet != 0)
		RenderPet(ClientData.m_RenderPos, m_Time);
	if(g_Config.m_InvalidEars != 0)
		RenderEars(ClientData.m_RenderPos, m_Time);
}

void CInvalid::RenderPet(const vec2 &Position, float Time)
{
	const float Orbit = 46.0f;
	const float Angle = Time * 1.8f;
	const vec2 Offset(std::cos(Angle) * Orbit, -28.0f + std::sin(Angle) * 10.0f);
	const vec2 PetPos = Position + Offset;

	ColorRGBA Color = ColorRGBA(0.7f, 0.5f, 1.0f, 0.9f);
	if(g_Config.m_InvalidPet == 1)
		Color = ColorRGBA(0.3f, 0.8f, 1.0f, 0.9f);
	else if(g_Config.m_InvalidPet == 2)
		Color = ColorRGBA(1.0f, 0.9f, 0.4f, 0.9f);
	else if(g_Config.m_InvalidPet == 3)
		Color = ColorRGBA(std::fabs(std::sin(Time * 2.0f)), std::fabs(std::sin(Time * 2.0f + 2.0f)), std::fabs(std::sin(Time * 2.0f + 4.0f)), 0.9f);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	const float Radius = 7.0f + std::sin(Time * 4.0f) * 1.5f;
	Graphics()->DrawCircle(PetPos.x, PetPos.y, Radius, 24);
	Graphics()->QuadsEnd();
}

void CInvalid::RenderEars(const vec2 &Position, float Time)
{
	ColorRGBA Color = ColorRGBA(0.9f, 0.7f, 0.4f, 0.95f);
	switch(g_Config.m_InvalidEars)
	{
	case 1:
		Color = ColorRGBA(0.95f, 0.65f, 0.35f, 0.95f);
		break;
	case 2:
		Color = ColorRGBA(0.95f, 0.95f, 0.95f, 0.95f);
		break;
	case 3:
		Color = ColorRGBA(0.75f, 0.45f, 0.2f, 0.95f);
		break;
	case 4:
		Color = ColorRGBA(0.85f, 0.2f, 0.25f, 0.95f);
		break;
	case 5:
		Color = ColorRGBA(0.3f, 0.9f, 0.8f, 0.95f);
		break;
	default:
		break;
	}

	const float Wobble = std::sin(Time * 3.0f) * 1.5f;
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	Graphics()->DrawCircle(Position.x - 9.0f, Position.y - 30.0f + Wobble, 5.0f, 16);
	Graphics()->DrawCircle(Position.x + 9.0f, Position.y - 30.0f + Wobble, 5.0f, 16);
	Graphics()->QuadsEnd();
}

void CInvalid::RenderPrank()
{
	const int LocalId = LocalClientId();
	if(LocalId < 0 || LocalId >= MAX_CLIENTS)
		return;

	const vec2 Pos = GameClient()->m_aClients[LocalId].m_RenderPos;
	const float Drip = std::fmod(m_Time * 55.0f, 20.0f);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.45f, 0.50f, 0.60f, 0.85f);
	Graphics()->DrawCircle(Pos.x, Pos.y - 48.0f, 14.0f, 20);
	Graphics()->DrawCircle(Pos.x - 11.0f, Pos.y - 43.0f, 10.0f, 20);
	Graphics()->DrawCircle(Pos.x + 11.0f, Pos.y - 43.0f, 10.0f, 20);

	Graphics()->SetColor(0.40f, 0.70f, 1.00f, 0.9f);
	for(int i = -3; i <= 3; ++i)
	{
		const float X = Pos.x + i * 6.0f;
		const float Y = Pos.y - 30.0f + std::fmod(Drip + i * 3.0f, 20.0f);
		IGraphics::CQuadItem Drop(X, Y, 1.5f, 6.0f);
		Graphics()->QuadsDraw(&Drop, 1);
	}
	Graphics()->QuadsEnd();
}

const char *CInvalid::VerdictLabel(EVerdict Verdict)
{
	switch(Verdict)
	{
	case EVerdict::LEGIT:
		return "легит";
	case EVerdict::SUSPICIOUS:
		return "мутный";
	case EVerdict::SOFT:
		return "софт";
	case EVerdict::NEUTRAL:
	default:
		return "нет данных";
	}
}

void CInvalid::RenderOverlay()
{
	if(m_vAssessments.empty())
		return;
	if(g_Config.m_InvalidOverlay == 1 && m_vAssessments.size() > 6)
		return;

	Ui()->MapScreen();
	const CUIRect *pScreen = Ui()->Screen();
	const float FontSize = 9.0f;
	float Y = 10.0f;

	for(const CAssessment &Assessment : m_vAssessments)
	{
		char aLine[256];
		str_format(aLine, sizeof(aLine), "%s — %s (%d%%)", Assessment.m_aName, VerdictLabel(Assessment.m_Verdict), (int)(Assessment.m_Score * 100.0f));
		const float Width = TextRender()->TextWidth(FontSize, aLine) + 10.0f;
		const float X = pScreen->w - 10.0f - Width;

		ColorRGBA BoxColor = ColorRGBA(0.1f, 0.1f, 0.1f, 0.65f);
		if(Assessment.m_Verdict == EVerdict::SUSPICIOUS)
			BoxColor = ColorRGBA(0.45f, 0.35f, 0.05f, 0.75f);
		else if(Assessment.m_Verdict == EVerdict::SOFT)
			BoxColor = ColorRGBA(0.5f, 0.1f, 0.1f, 0.8f);
		else if(Assessment.m_Verdict == EVerdict::LEGIT)
			BoxColor = ColorRGBA(0.1f, 0.35f, 0.15f, 0.7f);

		CUIRect Box = CUIRect{X, Y, Width, 13.0f};
		Box.Draw(BoxColor, IGraphics::CORNER_ALL, 3.0f);
		TextRender()->Text(X + 5.0f, Y + 1.0f, FontSize, aLine);
		Y += 15.0f;
	}
}
