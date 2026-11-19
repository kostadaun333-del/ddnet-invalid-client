/* invalid — клиентский компонент для DDNet.
 * Лицензия: GPL-3.0-or-later (совместимо с лицензией DDNet).
 *
 * ВАЖНО: оценка «на сколько процентов игрок читер» является эвристической.
 * Она не доказательство и не должна использоваться для публичных обвинений.
 */
#ifndef GAME_CLIENT_COMPONENTS_INVALID_H
#define GAME_CLIENT_COMPONENTS_INVALID_H

#include <base/types.h>
#include <base/vmath.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/shared/demo.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>

#include <array>
#include <vector>

class CInvalid : public CComponent
{
public:
	enum class EVerdict
	{
		NEUTRAL = 0,
		LEGIT,
		SUSPICIOUS,
		SOFT,
	};

	struct CAssessment
	{
		int m_ClientId = -1;
		char m_aName[MAX_NAME_LENGTH];
		float m_Score = 0.0f;
		EVerdict m_Verdict = EVerdict::NEUTRAL;
		char m_aReason[256];
		bool m_bHasData = false;

		CAssessment()
		{
			m_aName[0] = 0;
			m_aReason[0] = 0;
		}
	};

	CInvalid();
	int Sizeof() const override { return sizeof(*this); }

	void OnInit() override;
	void OnShutdown() override;
	void OnConsoleInit() override;
	void OnReset() override;
	void OnRender() override;

	void OpenMenu();
	void RunLocalAnalysis();
	void SpectatePlayer(int ClientId);
	void TriggerPrank();
	void LoadDemoAndAnalyze(const char *pPath);
	void SetDemoPath(const char *pPath);

	// Рисуется из CMenus::Render() после фона меню, до всех виджетов.
	void RenderMenuBackground();
	CLineInputBuffered<IO_MAX_PATH_LENGTH> &DemoPathInput() { return m_DemoPathInput; }
	CLineInputBuffered<IO_MAX_PATH_LENGTH> &PetImageInput() { return m_PetImageInput; }
	const char *Status() const { return m_aStatus; }
	bool IsAnalyzing() const { return m_bAnalyzing; }

	const std::vector<CAssessment> &Assessments() const { return m_vAssessments; }
	static const char *VerdictLabel(EVerdict Verdict);

private:
	class CDemoAnalysisListener : public CDemoPlayer::IListener
	{
	public:
		CDemoAnalysisListener(CInvalid *pSelf, CDemoPlayer *pPlayer) :
			m_pSelf(pSelf), m_pPlayer(pPlayer)
		{
		}
		void OnDemoPlayerSnapshot(void *pData, int Size) override;
		void OnDemoPlayerMessage(void *pData, int Size) override;

	private:
		CInvalid *m_pSelf;
		CDemoPlayer *m_pPlayer;
	};

	struct SPlayerSample
	{
		int m_Tick = 0;
		vec2 m_Pos = vec2(0.0f, 0.0f); // сырые единицы 1/32 блока
		vec2 m_Vel = vec2(0.0f, 0.0f);
		vec2 m_Aim = vec2(1.0f, 0.0f);
		int m_Weapon = 0;
		int m_FreezeEnd = 0;
		int m_FreezeStart = 0;
		bool m_InFreeze = false;
	};

	struct SProjectileSample
	{
		int m_Tick = 0;
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_Vel = vec2(0.0f, 0.0f);
	};

	static void ConInvalidMenu(IConsole::IResult *pResult, void *pUserData);
	static void ConInvalidPrank(IConsole::IResult *pResult, void *pUserData);
	static void ConInvalidAnalyze(IConsole::IResult *pResult, void *pUserData);
	static void ConInvalidSpectate(IConsole::IResult *pResult, void *pUserData);
	static void ConInvalidPet(IConsole::IResult *pResult, void *pUserData);
	static void ConInvalidEars(IConsole::IResult *pResult, void *pUserData);
	static void ConInvalidLoadDemo(IConsole::IResult *pResult, void *pUserData);

	void ProcessDemoSnapshot(void *pData, int Size, int Tick);
	void FinishAnalysis();
	void ComputeHeuristics();
	void RenderOverlay();
	void RenderCosmetics();
	void RenderPrank();
	void RenderPet(const vec2 &Position, float Time);
	void RenderEars(const vec2 &Position, float Time);
	void UpdatePetTexture();
	int LocalClientId() const;

	std::vector<CAssessment> m_vAssessments;
	std::vector<SPlayerSample> m_avSamples[MAX_CLIENTS];
	std::vector<SProjectileSample> m_vProjectiles;
	char m_aaNames[MAX_CLIENTS][MAX_NAME_LENGTH];
	float m_PrankUntil = 0.0f;
	float m_Time = 0.0f;
	float m_BackgroundTime = 0.0f;
	bool m_bAnalyzing = false;
	char m_aStatus[256];
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_DemoPathInput;
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_PetImageInput;
	IGraphics::CTextureHandle m_PetTexture;
	char m_aLoadedPetPath[IO_MAX_PATH_LENGTH];
};

#endif
