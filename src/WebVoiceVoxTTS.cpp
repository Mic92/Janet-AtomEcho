#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "WebVoiceVoxRootCA.h"
#include <AudioGeneratorWAV.h>
#include <AudioFileSourceBuffer.h>
#include "AudioFileSourceHTTPSStream.h"

//-----------------------------------------------------
extern String VOICEVOX_API_KEY;
extern AudioGeneratorWAV *wav;
extern AudioFileSourceBuffer *buff;
extern AudioFileSourceHTTPSStream *file;
extern int preallocateBufferSize;
extern uint8_t *preallocateBuffer;
void StatusCallback(void *cbData, int code, const char *string);
void playWAV(AudioFileSourceBuffer *buff);
//-----------------------------------------------------


void Voicevox_tts(char *text,char *voice){

  String tts_url = String("http://192.168.178.71:8081/api/tts");
  std::string json_body = "";

  DynamicJsonDocument doc(1024);
  doc["text"] = text;
  doc["voice"] = voice;
  serializeJson(doc, json_body);

  if(tts_url == "") return;
  file = new AudioFileSourceHTTPSStream(tts_url.c_str(), json_body, root_ca);
  buff = new AudioFileSourceBuffer(file, 10240);
  playWAV(buff);
}
