#include "AudioFileSourceChunkedHTTPStream.h"
#include <ArduinoJson.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorWAV.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

//-----------------------------------------------------
extern String VOICEVOX_API_KEY;
extern AudioGeneratorWAV *wav;
extern AudioFileSourceBuffer *buff;
extern AudioFileSourceChunkedHTTPStream *file;
extern int preallocateBufferSize;
extern uint8_t *preallocateBuffer;
void StatusCallback(void *cbData, int code, const char *string);
void playWAV(AudioFileSourceBuffer *buff);
//-----------------------------------------------------

void Piper_tts(const char *tts_url, const char *text, const char *voice) {
  DynamicJsonDocument doc(2048);
  doc["text"] = text;
  doc["voice"] = voice;
  std::string json_body = "";
  serializeJson(doc, json_body);

  file = new AudioFileSourceChunkedHTTPStream(tts_url, json_body);
  buff = new AudioFileSourceBuffer(file, 10240);
  playWAV(buff);
}
