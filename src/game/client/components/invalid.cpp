/* invalid — клиентский компонент для DDNet. */
#include "invalid.h"

#include <base/color.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <engine/shared/snapshot.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/protocol.h>
#include <generated/protocol7.h>

#include <game/client/components/menus.h>
#include <game/client/components/spectator.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace
{
float Frand(int Seed)
{
	const float X = std::sin((float)Seed * 12.9898f) * 43758.5453f;
	return X - std::floor(X);
}

float AngleBetween(vec2 A, vec2 B)
{
	const float Dot = std::clamp(A.x * B.x + A.y * B.y, -1.0f, 1.0f);
	return std::acos(Dot);
}

} // namespace

void CInvalid::CDemoAnalysisListener::OnDemoPlayerSnapshot(void *pData, int Size)
{
	m_pSelf->ProcessDemoSnapshot(pData, Size, m_pPlayer->Info()->m_Info.m_CurrentTick);
}

void CInvalid::CDemoAnalysisListener::OnDemoPlayerMessage(void *pData, int Size)
{
	(void)pData;
	(void)Size;
}

CInvalid::CInvalid()
{
	mem_zero(m_aaNames, sizeof(m_aaNames));
	mem_zero(m_aStatus, sizeof(m_aStatus));
	mem_zero(m_aLoadedPetPath, sizeof(m_aLoadedPetPath));
	str_copy(m_aStatus, "Демо не загружено.", sizeof(m_aStatus));
	OnReset();
}

void CInvalid::OnInit()
{
	m_DemoPathInput.Set("");
	m_PetImageInput.Set(g_Config.m_InvalidPetImage);
}

void CInvalid::OnShutdown()
{
	if(m_PetTexture.IsValid())
		Graphics()->UnloadTexture(&m_PetTexture);
}

void CInvalid::OnReset()
{
	m_vAssessments.clear();
	m_PrankUntil = 0.0f;
	m_Time = 0.0f;
	m_bAnalyzing = false;
	for(auto &vSamples : m_avSamples)
		vSamples.clear();
	m_vProjectiles.clear();
}

void CInvalid::OnConsoleInit()
{
	Console()->Register("invalid_menu", "", CFGFLAG_CLIENT, ConInvalidMenu, this, "Открыть меню invalid");
	Console()->Register("invalid_prank", "", CFGFLAG_CLIENT, ConInvalidPrank, this, "Локальный пранк: дождевое облачко над головой");
	Console()->Register("invalid_analyze", "", CFGFLAG_CLIENT, ConInvalidAnalyze, this, "Предварительный анализ игроков в текущем снимке");
	Console()->Register("invalid_spectate", "i[client-id]", CFGFLAG_CLIENT, ConInvalidSpectate, this, "Спектатор за выбранным игроком");
	Console()->Register("invalid_pet", "i[mode]", CFGFLAG_CLIENT, ConInvalidPet, this, "Питомец: 0 — выкл, 1 — сфера, 2 — звезда, 3 — радуга");
	Console()->Register("invalid_ears", "i[mode]", CFGFLAG_CLIENT, ConInvalidEars, this, "Ушки: 0 — выкл, 1..5 — разные варианты");
	Console()->Register("invalid_load_demo", "r[file]", CFGFLAG_CLIENT, ConInvalidLoadDemo, this, "Загрузить демо и запустить эвристический анализ");
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

void CInvalid::ConInvalidLoadDemo(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CInvalid *>(pUserData)->LoadDemoAndAnalyze(pResult->GetString(0));
}

void CInvalid::OpenMenu()
{
	g_Config.m_UiSettingsPage = CMenus::SETTINGS_INVALID;
	GameClient()->m_Menus.SetActive(true);
}

void CInvalid::SetDemoPath(const char *pPath)
{
	if(pPath)
		m_DemoPathInput.Set(pPath);
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
		str_copy(Assessment.m_aReason, "Нет данных из демо. Загрузите .demo через поле пути или команду invalid_load_demo.", sizeof(Assessment.m_aReason));
		m_vAssessments.push_back(Assessment);
	}
	str_copy(m_aStatus, "Показаны игроки сервера. Для оценки загрузите демо.", sizeof(m_aStatus));
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

/* --------------------------- разбор демо --------------------------- */

void CInvalid::LoadDemoAndAnalyze(const char *pPath)
{
	if(!pPath || pPath[0] == '\0')
	{
		str_copy(m_aStatus, "Укажите путь к .demo файлу.", sizeof(m_aStatus));
		return;
	}

	SetDemoPath(pPath);
	m_bAnalyzing = true;
	str_copy(m_aStatus, "Читаю демо...", sizeof(m_aStatus));

	for(auto &vSamples : m_avSamples)
		vSamples.clear();
	m_vProjectiles.clear();
	mem_zero(m_aaNames, sizeof(m_aaNames));

	CSnapshotDelta Delta;
	CSnapshotDelta Delta7;
	{
		CNetObjHandler Handler;
		for(int i = 0; i < NUM_NETOBJTYPES; ++i)
			Delta.SetStaticsize(i, Handler.GetObjSize(i));
		protocol7::CNetObjHandler Handler7;
		for(int i = 0; i < 23; ++i)
			Delta7.SetStaticsize(i, Handler7.GetObjSize(i));
	}

	CDemoPlayer DemoPlayer(&Delta, &Delta7, false);
	if(DemoPlayer.Load(Storage(), pPath, IStorage::TYPE_ALL_OR_ABSOLUTE) == -1)
	{
		str_format(m_aStatus, sizeof(m_aStatus), "Не удалось открыть демо: %s", DemoPlayer.ErrorMessage() ? DemoPlayer.ErrorMessage() : "неизвестная ошибка");
		m_bAnalyzing = false;
		return;
	}

	CDemoAnalysisListener Listener(this, &DemoPlayer);
	DemoPlayer.SetListener(&Listener);
	CNetBase::Init();
	DemoPlayer.Play();

	int Iterations = 0;
	while(DemoPlayer.IsPlaying() && Iterations < 400000)
	{
		DemoPlayer.Update(false);
		if(DemoPlayer.Info()->m_Info.m_Paused)
			break;
		++Iterations;
	}
	DemoPlayer.Stop();
	m_bAnalyzing = false;

	FinishAnalysis();
}

void CInvalid::ProcessDemoSnapshot(void *pData, int Size, int Tick)
{
	CSnapshot *pSnap = static_cast<CSnapshot *>(pData);
	if(!pSnap || !pSnap->IsValid((size_t)Size))
		return;

	CSnapshotBuffer UnpackedBuffer;
	{
		CUnpacker Unpacker;
		CSnapshotBuilder Builder;
		CNetObjHandler Handler;
		Builder.Init();
		for(int Index = 0; Index < pSnap->NumItems(); ++Index)
		{
			const CSnapshotItem *pItem = pSnap->GetItem(Index);
			const int ItemType = pSnap->GetItemType(Index);
			if(ItemType <= 0)
				continue;
			Unpacker.Reset(pItem->Data(), pSnap->GetItemSize(Index));
			void *pSecured = Handler.SecureUnpackObj(ItemType, &Unpacker);
			if(!pSecured)
				continue;
			const int ItemSize = Handler.GetUnpackedObjSize(ItemType);
			if(!Builder.NewItem(ItemType, pItem->Id(), pSecured, ItemSize))
				return;
		}
		if(Builder.Finish(&UnpackedBuffer) < 0)
			return;
	}

	const CSnapshot *pUnpacked = UnpackedBuffer.AsSnapshot();
	for(int Index = 0; Index < pUnpacked->NumItems(); ++Index)
	{
		const CSnapshotItem *pItem = pUnpacked->GetItem(Index);
		const int Type = pUnpacked->GetItemType(Index);
		const int Id = pItem->Id();
		const void *pObj = pItem->Data();

		if(Type == NETOBJTYPE_CLIENTINFO && Id >= 0 && Id < MAX_CLIENTS)
		{
			const CNetObj_ClientInfo *pInfo = static_cast<const CNetObj_ClientInfo *>(pObj);
			IntsToStr(pInfo->m_aName, std::size(pInfo->m_aName), m_aaNames[Id], sizeof(m_aaNames[Id]));
		}
		else if(Type == NETOBJTYPE_CHARACTER && Id >= 0 && Id < MAX_CLIENTS)
		{
			const CNetObj_Character *pChar = static_cast<const CNetObj_Character *>(pObj);
			SPlayerSample Sample;
			Sample.m_Tick = Tick;
			Sample.m_Pos = vec2(pChar->m_X, pChar->m_Y);
			Sample.m_Aim = direction((float)pChar->m_Angle / 256.0f);
			Sample.m_Weapon = pChar->m_Weapon;
			if(!m_avSamples[Id].empty())
			{
				const SPlayerSample &Prev = m_avSamples[Id].back();
				const float Dt = (float)(Tick - Prev.m_Tick);
				if(Dt > 0.0f)
					Sample.m_Vel = (Sample.m_Pos - Prev.m_Pos) / Dt;
			}
			m_avSamples[Id].push_back(Sample);
		}
		else if(Type == NETOBJTYPE_DDNETCHARACTER && Id >= 0 && Id < MAX_CLIENTS && !m_avSamples[Id].empty())
		{
			const CNetObj_DDNetCharacter *pExtra = static_cast<const CNetObj_DDNetCharacter *>(pObj);
			SPlayerSample &Sample = m_avSamples[Id].back();
			Sample.m_FreezeEnd = pExtra->m_FreezeEnd;
			Sample.m_FreezeStart = pExtra->m_FreezeStart;
			Sample.m_InFreeze = (pExtra->m_Flags & CHARACTERFLAG_IN_FREEZE) != 0;
		}
		else if(Type == NETOBJTYPE_PROJECTILE)
		{
			const CNetObj_Projectile *pProj = static_cast<const CNetObj_Projectile *>(pObj);
			SProjectileSample Sample;
			Sample.m_Tick = Tick;
			Sample.m_Pos = vec2(pProj->m_X, pProj->m_Y);
			Sample.m_Vel = vec2(pProj->m_VelX, pProj->m_VelY) / 256.0f;
			m_vProjectiles.push_back(Sample);
		}
	}
}

void CInvalid::ComputeHeuristics()
{
	m_vAssessments.clear();
	int Analyzed = 0;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		const std::vector<SPlayerSample> &v = m_avSamples[ClientId];
		if(v.size() < 50)
			continue;
		++Analyzed;

		CAssessment A;
		A.m_ClientId = ClientId;
		if(m_aaNames[ClientId][0] != '\0')
			str_copy(A.m_aName, m_aaNames[ClientId], sizeof(A.m_aName));
		else
			str_format(A.m_aName, sizeof(A.m_aName), "Игрок %d", ClientId);

		float Score = 0.0f;
		char aReasons[256] = "";
		int NumReasons = 0;

		// 1. Резкие доводки прицела по ближайшему игроку.
		int OnTarget = 0;
		int Snaps = 0;
		int SnapToTarget = 0;
		const size_t Start = std::min<size_t>(20, v.size() - 1);
		for(size_t i = Start; i < v.size(); ++i)
		{
			const SPlayerSample &S = v[i];
			if(S.m_InFreeze)
				continue;
			vec2 Closest(0.0f, 0.0f);
			float BestDist = 1400.0f;
			for(int Other = 0; Other < MAX_CLIENTS; ++Other)
			{
				if(Other == ClientId || m_avSamples[Other].size() < v.size())
					continue;
				const vec2 Delta = m_avSamples[Other][i].m_Pos - S.m_Pos;
				const float Dist = std::sqrt(Delta.x * Delta.x + Delta.y * Delta.y);
				if(Dist > 1.0f && Dist < BestDist)
				{
					BestDist = Dist;
					Closest = Delta / Dist;
				}
			}
			if(BestDist < 1400.0f && AngleBetween(S.m_Aim, Closest) < 0.12f)
				++OnTarget;

			if(i > 0 && !v[i - 1].m_InFreeze)
			{
				const float Turn = AngleBetween(S.m_Aim, v[i - 1].m_Aim);
				if(Turn > 0.85f)
				{
					++Snaps;
					if(BestDist < 1400.0f && AngleBetween(S.m_Aim, Closest) < 0.2f)
						++SnapToTarget;
				}
			}
		}
		const float Samples = (float)(v.size() - Start);
		const float AimRatio = Samples > 0.0f ? (float)OnTarget / Samples : 0.0f;
		const float SnapRatio = Snaps > 0 ? (float)SnapToTarget / (float)Snaps : 0.0f;
		if(Samples > 0.0f && AimRatio > 0.35f)
		{
			Score += 18.0f * AimRatio + 22.0f * SnapRatio;
			if(AimRatio > 0.5f || SnapRatio > 0.3f)
			{
				str_format(aReasons + std::strlen(aReasons), sizeof(aReasons) - std::strlen(aReasons),
					"%sрезкие доводки прицела (совпадение %d%%, доводки %d)", NumReasons++ ? "; " : "",
					(int)(AimRatio * 100.0f), Snaps);
			}
		}

		// 2. Уклонение от снарядов: резкая смена скорости рядом с летящим снарядом.
		int Avoids = 0;
		for(const SProjectileSample &P : m_vProjectiles)
		{
			for(size_t i = Start; i + 3 < v.size(); ++i)
			{
				const SPlayerSample &S = v[i];
				if(std::abs(S.m_Tick - P.m_Tick) > 8)
					continue;
				const vec2 Delta = S.m_Pos - P.m_Pos;
				const float Dist = std::sqrt(Delta.x * Delta.x + Delta.y * Delta.y);
				if(Dist > 220.0f)
					continue;
				const vec2 PrevVel = v[i - 1].m_Vel;
				const vec2 NextVel = v[i + 3].m_Vel;
				if(AngleBetween(PrevVel, NextVel) > 1.0f && PrevVel.x * PrevVel.x + PrevVel.y * PrevVel.y > 100.0f)
				{
					++Avoids;
					break;
				}
			}
		}
		if(Avoids >= 2)
		{
			Score += std::min(20.0f, (float)Avoids * 1.5f);
			str_format(aReasons + std::strlen(aReasons), sizeof(aReasons) - std::strlen(aReasons),
				"%sуклонение от снарядов (%d)", NumReasons++ ? "; " : "", Avoids);
		}

		// 3. Аим по молоту: прицел на близкого игрока с молотом в руках.
		int HammerSamples = 0;
		int HammerOnTarget = 0;
		for(size_t i = Start; i < v.size(); ++i)
		{
			const SPlayerSample &S = v[i];
			if(S.m_Weapon != WEAPON_HAMMER || S.m_InFreeze)
				continue;
			++HammerSamples;
			for(int Other = 0; Other < MAX_CLIENTS; ++Other)
			{
				if(Other == ClientId || m_avSamples[Other].size() < v.size())
					continue;
				const vec2 Delta = m_avSamples[Other][i].m_Pos - S.m_Pos;
				const float Dist = std::sqrt(Delta.x * Delta.x + Delta.y * Delta.y);
				if(Dist > 1.0f && Dist < 200.0f && AngleBetween(S.m_Aim, Delta / Dist) < 0.1f)
				{
					++HammerOnTarget;
					break;
				}
			}
		}
		if(HammerSamples > 30 && (float)HammerOnTarget / (float)HammerSamples > 0.65f)
		{
			Score += 16.0f;
			str_format(aReasons + std::strlen(aReasons), sizeof(aReasons) - std::strlen(aReasons),
				"%sаим по молоту (%d%%)", NumReasons++ ? "; " : "",
				(int)(100.0f * (float)HammerOnTarget / (float)HammerSamples));
		}

		// 4. Автоанфриз: одинаково короткие интервалы заморозки.
		std::vector<int> vFreezeDurations;
		for(size_t i = 1; i < v.size(); ++i)
		{
			if(v[i - 1].m_InFreeze && !v[i].m_InFreeze && v[i - 1].m_FreezeStart > 0)
				vFreezeDurations.push_back(v[i].m_Tick - v[i - 1].m_FreezeStart);
		}
		if(vFreezeDurations.size() >= 3)
		{
			float Sum = 0.0f;
			for(int D : vFreezeDurations)
				Sum += (float)D;
			const float Mean = Sum / (float)vFreezeDurations.size();
			float Var = 0.0f;
			for(int D : vFreezeDurations)
				Var += ((float)D - Mean) * ((float)D - Mean);
			const float Dev = std::sqrt(Var / (float)vFreezeDurations.size());
			if(Dev < 1.5f && Mean < 12.0f)
			{
				Score += 25.0f;
				str_format(aReasons + std::strlen(aReasons), sizeof(aReasons) - std::strlen(aReasons),
					"%sавтоанфриз (в среднем %.1f тика, разброс %.1f)", NumReasons++ ? "; " : "", Mean, Dev);
			}
		}

		A.m_Score = std::clamp(Score, 0.0f, 100.0f);
		A.m_bHasData = true;
		A.m_Verdict = A.m_Score >= 60.0f ? EVerdict::SOFT : (A.m_Score >= 25.0f ? EVerdict::SUSPICIOUS : EVerdict::LEGIT);
		if(NumReasons == 0)
			str_copy(A.m_aReason, "Подозрительных паттернов не обнаружено.", sizeof(A.m_aReason));
		else
			str_copy(A.m_aReason, aReasons, sizeof(A.m_aReason));
		m_vAssessments.push_back(A);
	}

	if(Analyzed == 0)
		str_copy(m_aStatus, "В демо не найдено игроков с достаточным числом снимков.", sizeof(m_aStatus));
	else
		str_format(m_aStatus, sizeof(m_aStatus), "Проанализировано игроков: %d. Оценка эвристическая, не доказательство.", Analyzed);
}

void CInvalid::FinishAnalysis()
{
	ComputeHeuristics();
}

/* --------------------------- рендер --------------------------- */

void CInvalid::OnRender()
{
	m_Time = Client()->GlobalTime();
	m_BackgroundTime = m_Time;
	UpdatePetTexture();

	if(!g_Config.m_InvalidEnabled)
		return;

	if(g_Config.m_InvalidRain == 2)
		RenderMenuBackground();

	const int State = Client()->State();
	if(State != IClient::STATE_ONLINE && State != IClient::STATE_DEMOPLAYBACK)
		return;

	if(g_Config.m_InvalidPet || g_Config.m_InvalidEars)
		RenderCosmetics();
	if(m_Time < m_PrankUntil)
		RenderPrank();

	if(g_Config.m_InvalidOverlay != 0)
		RenderOverlay();
}

void CInvalid::UpdatePetTexture()
{
	const char *pTyped = m_PetImageInput.GetString();
	if(pTyped && std::strcmp(pTyped, g_Config.m_InvalidPetImage) != 0)
		str_copy(g_Config.m_InvalidPetImage, pTyped, sizeof(g_Config.m_InvalidPetImage));

	const char *pPath = g_Config.m_InvalidPetImage;
	if(!pPath || pPath[0] == '\0')
	{
		if(m_PetTexture.IsValid())
		{
			Graphics()->UnloadTexture(&m_PetTexture);
			m_aLoadedPetPath[0] = '\0';
		}
		return;
	}
	if(std::strcmp(m_aLoadedPetPath, pPath) == 0)
		return;

	if(m_PetTexture.IsValid())
		Graphics()->UnloadTexture(&m_PetTexture);
	m_PetTexture = Graphics()->LoadTexture(pPath, IStorage::TYPE_ALL_OR_ABSOLUTE, Graphics()->TextureLoadFlags());
	if(m_PetTexture.IsValid())
	{
		str_copy(m_aLoadedPetPath, pPath, sizeof(m_aLoadedPetPath));
		str_copy(m_aStatus, "Картинка питомца загружена.", sizeof(m_aStatus));
	}
	else
	{
		m_aLoadedPetPath[0] = '\0';
		str_copy(m_aStatus, "Не удалось загрузить картинку питомца.", sizeof(m_aStatus));
	}
}

void CInvalid::RenderMenuBackground()
{
	if(!g_Config.m_InvalidEnabled || g_Config.m_InvalidRain == 0)
		return;

	Ui()->MapScreen();
	const CUIRect *pScreen = Ui()->Screen();
	const float T = m_BackgroundTime;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(ColorRGBA(0.55f, 0.75f, 1.0f, 0.28f));
	for(int i = 0; i < 110; ++i)
	{
		const float Speed = 260.0f + Frand(i * 3 + 1) * 420.0f;
		const float X = Frand(i * 7 + 2) * pScreen->w;
		const float Y = std::fmod(T * Speed + Frand(i * 11 + 3) * (pScreen->h + 80.0f), pScreen->h + 80.0f) - 40.0f;
		IGraphics::CQuadItem Drop(X, Y, 1.6f, 13.0f + Frand(i * 5) * 8.0f);
		Graphics()->QuadsDraw(&Drop, 1);
	}
	Graphics()->QuadsEnd();

	if(!g_Config.m_InvalidLightning)
		return;

	const float Cycle = std::fmod(T, 9.0f);
	if(Cycle > 0.18f)
		return;

	const int Seed = (int)(T / 9.0f);
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(ColorRGBA(0.85f, 0.92f, 1.0f, 0.16f * (1.0f - Cycle / 0.18f)));
	IGraphics::CQuadItem Flash(pScreen->w * 0.5f, pScreen->h * 0.5f, pScreen->w, pScreen->h);
	Graphics()->QuadsDraw(&Flash, 1);

	Graphics()->SetColor(ColorRGBA(0.95f, 0.97f, 1.0f, 0.9f));
	vec2 P(Frand(Seed + 17) * pScreen->w, -10.0f);
	for(int i = 0; i < 9; ++i)
	{
		vec2 Next(P.x + (Frand(Seed * 7 + i) - 0.5f) * 110.0f, P.y + pScreen->h * 0.055f);
		const vec2 Dir = Next - P;
		const float Len = std::sqrt(Dir.x * Dir.x + Dir.y * Dir.y);
		if(Len > 0.001f)
		{
			const vec2 Normal = vec2(-Dir.y / Len, Dir.x / Len) * 1.2f;
			IGraphics::CFreeformItem Bolt(P + Normal, Next + Normal, P - Normal, Next - Normal);
			Graphics()->QuadsDrawFreeform(&Bolt, 1);
		}
		P = Next;
	}
	Graphics()->QuadsEnd();
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

	if(m_PetTexture.IsValid())
	{
		Graphics()->TextureSet(m_PetTexture);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.95f));
		Graphics()->QuadsSetRotation(std::sin(Time * 2.0f) * 0.18f);
		const float Size = 24.0f + std::sin(Time * 4.0f) * 2.0f;
		IGraphics::CQuadItem Quad(PetPos.x, PetPos.y, Size, Size);
		Graphics()->QuadsDrawTL(&Quad, 1);
		Graphics()->QuadsEnd();
		Graphics()->QuadsSetRotation(0.0f);
		return;
	}

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	if(g_Config.m_InvalidPet == 1)
	{
		Graphics()->SetColor(ColorRGBA(0.3f, 0.8f, 1.0f, 0.9f));
		Graphics()->DrawCircle(PetPos.x, PetPos.y, 7.0f + std::sin(Time * 4.0f) * 1.5f, 24);
	}
	else if(g_Config.m_InvalidPet == 2)
	{
		Graphics()->SetColor(ColorRGBA(1.0f, 0.85f, 0.25f, 0.95f));
		for(int i = 0; i < 5; ++i)
		{
			const float A = Time * 0.8f + i * (2.0f * std::acos(-1.0f) / 5.0f);
			const vec2 Inner = PetPos + vec2(std::cos(A + 0.6f) * 4.0f, std::sin(A + 0.6f) * 4.0f);
			const vec2 Outer1 = PetPos + vec2(std::cos(A) * 12.0f, std::sin(A) * 12.0f);
			const vec2 Outer2 = PetPos + vec2(std::cos(A + 1.2566f) * 12.0f, std::sin(A + 1.2566f) * 12.0f);
			IGraphics::CFreeformItem Tri(PetPos, Outer1, Inner, Outer2);
			Graphics()->QuadsDrawFreeform(&Tri, 1);
		}
	}
	else
	{
		const float Colors[][3] = {{1.0f, 0.3f, 0.3f}, {1.0f, 0.7f, 0.2f}, {0.9f, 1.0f, 0.3f}, {0.3f, 0.9f, 0.5f}, {0.3f, 0.6f, 1.0f}};
		for(int i = 0; i < 5; ++i)
		{
			const float A = Time * 1.3f + i * (2.0f * std::acos(-1.0f) / 5.0f);
			const vec2 P = PetPos + vec2(std::cos(A) * 9.0f, std::sin(A) * 9.0f);
			Graphics()->SetColor(Colors[i][0], Colors[i][1], Colors[i][2], 0.9f);
			Graphics()->DrawCircle(P.x, P.y, 4.5f, 16);
		}
	}
	Graphics()->QuadsEnd();
}

void CInvalid::RenderEars(const vec2 &Position, float Time)
{
	ColorRGBA Color = ColorRGBA(0.9f, 0.7f, 0.4f, 0.95f);
	switch(g_Config.m_InvalidEars)
	{
	case 1: Color = ColorRGBA(0.95f, 0.65f, 0.35f, 0.95f); break;
	case 2: Color = ColorRGBA(0.95f, 0.95f, 0.95f, 0.95f); break;
	case 3: Color = ColorRGBA(0.75f, 0.45f, 0.2f, 0.95f); break;
	case 4: Color = ColorRGBA(0.85f, 0.2f, 0.25f, 0.95f); break;
	case 5: Color = ColorRGBA(0.3f, 0.9f, 0.8f, 0.95f); break;
	default: break;
	}

	const float Wobble = std::sin(Time * 3.0f) * 1.5f;
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	for(int Side = -1; Side <= 1; Side += 2)
	{
		const vec2 Base(Position.x + Side * 10.0f, Position.y - 26.0f + Wobble);
		const vec2 Tip(Position.x + Side * 16.0f, Position.y - 44.0f + Wobble);
		const vec2 Inner(Position.x + Side * 3.0f, Position.y - 28.0f + Wobble);
		IGraphics::CFreeformItem Ear(Base, Tip, Inner, Inner);
		Graphics()->QuadsDrawFreeform(&Ear, 1);
	}
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
	case EVerdict::LEGIT: return "легит";
	case EVerdict::SUSPICIOUS: return "мутный";
	case EVerdict::SOFT: return "софт";
	case EVerdict::NEUTRAL:
	default: return "нет данных";
	}
}

void CInvalid::RenderOverlay()
{
	Ui()->MapScreen();
	const CUIRect *pScreen = Ui()->Screen();
	const float FontSize = 9.0f;
	float Y = 10.0f;

	char aStatus[256];
	str_format(aStatus, sizeof(aStatus), "invalid: %s", m_aStatus);
	const float StatusWidth = TextRender()->TextWidth(FontSize, aStatus) + 10.0f;
	CUIRect StatusBox = CUIRect{pScreen->w - 10.0f - StatusWidth, Y, StatusWidth, 13.0f};
	StatusBox.Draw(ColorRGBA(0.08f, 0.08f, 0.12f, 0.75f), IGraphics::CORNER_ALL, 3.0f);
	TextRender()->Text(StatusBox.x + 5.0f, Y + 1.0f, FontSize, aStatus);
	Y += 16.0f;

	if(g_Config.m_InvalidOverlay == 1 && m_vAssessments.size() > 6)
		return;

	for(const CAssessment &A : m_vAssessments)
	{
		char aLine[300];
		if(g_Config.m_InvalidShowReasons && A.m_bHasData && A.m_aReason[0] != '\0')
			str_format(aLine, sizeof(aLine), "%s — %s (%d%%): %s", A.m_aName, VerdictLabel(A.m_Verdict), (int)(A.m_Score * 100.0f), A.m_aReason);
		else
			str_format(aLine, sizeof(aLine), "%s — %s (%d%%)", A.m_aName, VerdictLabel(A.m_Verdict), (int)(A.m_Score * 100.0f));

		const float Width = TextRender()->TextWidth(FontSize, aLine) + 10.0f;
		const float X = pScreen->w - 10.0f - Width;
		ColorRGBA BoxColor = ColorRGBA(0.1f, 0.1f, 0.1f, 0.7f);
		if(A.m_Verdict == EVerdict::SUSPICIOUS)
			BoxColor = ColorRGBA(0.45f, 0.35f, 0.05f, 0.78f);
		else if(A.m_Verdict == EVerdict::SOFT)
			BoxColor = ColorRGBA(0.5f, 0.1f, 0.1f, 0.82f);
		else if(A.m_Verdict == EVerdict::LEGIT)
			BoxColor = ColorRGBA(0.1f, 0.35f, 0.15f, 0.74f);

		CUIRect Box = CUIRect{X, Y, Width, 13.0f};
		Box.Draw(BoxColor, IGraphics::CORNER_ALL, 3.0f);
		TextRender()->Text(X + 5.0f, Y + 1.0f, FontSize, aLine);
		Y += 15.0f;
	}
}
