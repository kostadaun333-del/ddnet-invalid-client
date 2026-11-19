/* invalid — клиентский компонент для DDNet.
 * Лицензия: GPL-3.0-or-later (совместимо с лицензией DDNet).
 *
 * ВАЖНО: оценка «на сколько процентов игрок читер» является эвристической.
 * Она не доказательство и не должна использоваться для публичных обвинений.
 */
#ifndef GAME_CLIENT_COMPONENTS_INVALID_H
#define GAME_CLIENT_COMPONENTS_INVALID_H

#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <game/client/component.h>

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
		char m_aReason[160];
		bool m_bHasData = false;

		CAssessment()
		{
			m_aName[0] = 0;
			m_aReason[0] = 0;
		}
	};

	CInvalid();
	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	void OnReset() override;
	void OnRender() override;

	void OpenMenu();
	void RunLocalAnalysis();
	void SpectatePlayer(int ClientId);
	void TriggerPrank();

	const std::vector<CAssessment> &Assessments() const { return m_vAssessments; }
	static const char *VerdictLabel(EVerdict Verdict);

private:
	static void ConInvalidMenu(IConsole::IResult *pResult, void *pUserData);
	static void ConInvalidPrank(IConsole::IResult *pResult, void *pUserData);
	static void ConInvalidAnalyze(IConsole::IResult *pResult, void *pUserData);
	static void ConInvalidSpectate(IConsole::IResult *pResult, void *pUserData);
	static void ConInvalidPet(IConsole::IResult *pResult, void *pUserData);
	static void ConInvalidEars(IConsole::IResult *pResult, void *pUserData);

	void RenderOverlay();
	void RenderCosmetics();
	void RenderPrank();
	void RenderPet(const vec2 &Position, float Time);
	void RenderEars(const vec2 &Position, float Time);
	int LocalClientId() const;

	std::vector<CAssessment> m_vAssessments;
	float m_PrankUntil = 0.0f;
	float m_Time = 0.0f;
};

#endif
