/* invalid — локальная озвучка через голосовой движок ОС. */
#ifndef GAME_CLIENT_COMPONENTS_AI_VOICE_H
#define GAME_CLIENT_COMPONENTS_AI_VOICE_H

// Говорит текст локально: Windows SAPI, Linux espeak-ng/spd-say.
// Ничего не отправляет в сеть.
void AiSpeakLocal(const char *pText, int Volume);
void AiShutdownVoice();

#endif
