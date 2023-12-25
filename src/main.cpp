#include <Arduino.h>
#include <M5Unified.h>

#include <AudioOutput.h>
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputM5Speaker.h"
#include "AudioFileSourceHTTPSStream.h"
#include "WebVoiceVoxTTS.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "rootCACertificate.h"
#include "rootCAgoogle.h"
#include <ArduinoJson.h>
#include "AudioWhisper.h"
#include "Whisper.h"
#include "Audio.h"
#include "CloudSpeechClient.h"
#include <deque>
#include <FastLED.h>

const char *SSID = "";
const char *PASSWORD = "";

// Pin number for the LED strip
#define LED_PIN     27
// Number of LEDs in the LED strip
#define NUM_LEDS    1
// Brightness
#define BRIGHTNESS 180
// Color order of the LED strip
#define COLOR_ORDER GRB
// Initialization of the FastLED library
CRGB leds[NUM_LEDS];

// Maximum number of saved questions and answers
const int MAX_HISTORY = 5;

// Data structure to store past questions and answers
std::deque<String> chatHistory;

#define OPENAI_APIKEY ""

//---------------------------------------------
String OPENAI_API_KEY = "";
String TTS_VOICE = "en-us-libritts-high.onnx";

// CRGB::Pink
// CRGB::Yellow
// CRGB::MidnightBlue
// CRGB::Red
// CRGB::LightBlue
// CRGB::Orange
// CRGB::Magenta
// CRGB::MediumBlue
void set_led_color(CRGB col){
  leds[0] = col;
  FastLED.show();
}

/// set M5Speaker virtual channel (0-7)
static constexpr uint8_t m5spk_virtual_channel = 0;
static AudioOutputM5Speaker out(&M5.Speaker, m5spk_virtual_channel);
AudioGeneratorWAV *wav;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;

String speech_text = "";
String speech_text_buffer = "";
DynamicJsonDocument chat_doc(1024*10);
String json_ChatString = "{\"model\": \"gpt-3.5-turbo-1106\",\"messages\": [{\"role\": \"user\", \"content\": \"""\"}]}";
// String json_ChatString =
// "{\"model\": \"gpt-3.5-turbo\",\
//  \"messages\": [\
//                 {\"role\": \"user\", \"content\": \"" + text + "\"},\
//                 {\"role\": \"system\", \"content\": \"あなたは「スタックちゃん」と言う名前の小型ロボットとして振る舞ってください。\"},\
//                 {\"role\": \"system\", \"content\": \"あなたはの使命は人々の心を癒すことです。\"},\
//                 {\"role\": \"system\", \"content\": \"幼い子供の口調で話してください。\"},\
//                 {\"role\": \"system\", \"content\": \"語尾には「だよ｝をつけて話してください。\"}\
//               ]}";
String Role_JSON = "";
String InitBuffer = "";

bool init_chat_doc(const char *data)
{
  DeserializationError error = deserializeJson(chat_doc, data);
  if (error) {
    Serial.println("DeserializationError");
    return false;
  }
  String json_str; //= JSON.stringify(chat_doc);
  serializeJsonPretty(chat_doc, json_str);  // 文字列をシリアルポートに出力する
//  Serial.println(json_str);
    return true;
}

String https_post_json(const char* url, const char* json_string, const char* root_ca) {
  String payload = "";
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert(root_ca);
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
      HTTPClient https;
      https.setTimeout( 65000 );

      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, url)) {  // HTTPS
        Serial.print("[HTTPS] POST...\n");
        // start connection and send HTTP header
        https.addHeader("Content-Type", "application/json");
        https.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);
        int httpCode = https.POST((uint8_t *)json_string, strlen(json_string));

        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] POST... code: %d\n", httpCode);

          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            payload = https.getString();
            Serial.println("//////////////");
            Serial.println(payload);
            Serial.println("//////////////");
          }
        } else {
          Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
      // End extra scoping block
    }
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
  return payload;
}

String chatGpt(String json_string) {
  String response = "";
Serial.print("chatGpt = ");
Serial.println(json_string);
  // avatar.setExpression(Expression::Doubt);
  // avatar.setSpeechText("考え中…");
  String ret = https_post_json("https://api.openai.com/v1/chat/completions", json_string.c_str(), root_ca_openai);
  // avatar.setExpression(Expression::Neutral);
  // avatar.setSpeechText("");
  Serial.println(ret);
  if(ret != ""){
    DynamicJsonDocument doc(2000);
    DeserializationError error = deserializeJson(doc, ret.c_str());
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      // avatar.setExpression(Expression::Sad);
      // avatar.setSpeechText("エラーです");
      response = "Got an error";
      delay(1000);
      // avatar.setSpeechText("");
      // avatar.setExpression(Expression::Neutral);
    }else{
      const char* data = doc["choices"][0]["message"]["content"];
      Serial.println(data);
      response = String(data);
      std::replace(response.begin(),response.end(),'\n',' ');
    }
  } else {
    // avatar.setExpression(Expression::Sad);
    // avatar.setSpeechText("わかりません");
    response = "I do not understand";
    delay(1000);
    // avatar.setSpeechText("");
    // avatar.setExpression(Expression::Neutral);
  }
  return response;
}

String exec_chatGPT(String text) {
  static String response = "";
  Serial.println(InitBuffer);
  init_chat_doc(InitBuffer.c_str());
  chatHistory.push_back(text);
  if (chatHistory.size() > MAX_HISTORY * 2)
  {
    chatHistory.pop_front();
    chatHistory.pop_front();
  }

  for (int i = 0; i < chatHistory.size(); i++)
  {
    JsonArray messages = chat_doc["messages"];
    JsonObject systemMessage1 = messages.createNestedObject();
    if(i % 2 == 0) {
      systemMessage1["role"] = "user";
    } else {
      systemMessage1["role"] = "assistant";
    }
    systemMessage1["content"] = chatHistory[i];
  }

  String json_string;
  serializeJson(chat_doc, json_string);
    response = chatGpt(json_string);
//    speech_text = response;
    chatHistory.push_back(response);
  // Serial.printf("chatHistory.max_size %d \n",chatHistory.max_size());
  // Serial.printf("chatHistory.size %d \n",chatHistory.size());
  // for (int i = 0; i < chatHistory.size(); i++)
  // {
  //   Serial.print(i);
  //   Serial.println("= "+chatHistory[i]);
  // }
  serializeJsonPretty(chat_doc, json_string);
  Serial.println("====================");
  Serial.println(json_string);
  Serial.println("====================");

  return response;
}

void playWAV(AudioFileSourceBuffer *buff){
  wav->begin(buff, &out);
}

String SpeechToText(){
  Serial.println("\r\nRecord start!\r\n");

  String ret = "";
  AudioWhisper* audio = new AudioWhisper();
  audio->Record();
  Serial.println("Record end\r\n");
  //    avatar.setSpeechText("わかりました");
  set_led_color(CRGB::Orange);
  Whisper* cloudSpeechClient = new Whisper(root_ca_openai, OPENAI_API_KEY.c_str());
  ret = cloudSpeechClient->Transcribe(audio);
  delete cloudSpeechClient;
  delete audio;
  return ret;
}


// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2)-1]=0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}

void setup()
{
  auto cfg = M5.config();

  cfg.external_spk = true;    /// use external speaker (SPK HAT / ATOMIC SPK)
//cfg.external_spk_detail.omit_atomic_spk = true; // exclude ATOMIC SPK
//cfg.external_spk_detail.omit_spk_hat    = true; // exclude SPK HAT

  M5.begin(cfg);

  { /// custom setting
    auto spk_cfg = M5.Speaker.config();
    /// Increasing the sample_rate will improve the sound quality instead of increasing the CPU load.
    spk_cfg.sample_rate = 96000; // default:64000 (64kHz)  e.g. 48000 , 50000 , 80000 , 96000 , 100000 , 128000 , 144000 , 192000 , 200000
    spk_cfg.task_pinned_core = APP_CPU_NUM;
    M5.Speaker.config(spk_cfg);
  }
  M5.Speaker.begin();

  FastLED.addLeds<SK6812, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show();
  set_led_color(CRGB::Green);

  Serial.println("Connecting to WiFi");
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  M5.update();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("...Connecting to WiFi");
    delay(1000);
  }
  Serial.println("Connected");
  set_led_color(CRGB::Black);

  OPENAI_API_KEY = OPENAI_APIKEY;

  M5.Speaker.setVolume(200);
//   audioLogger = &Serial;
   wav = new AudioGeneratorWAV();
  init_chat_doc(json_ChatString.c_str());
  serializeJson(chat_doc, InitBuffer);
}


void loop()
{
  M5.update();
  if (M5.BtnA.wasPressed())
  {
    M5.Speaker.tone(1000, 100);
    delay(200);
    set_led_color(CRGB::Magenta);
    M5.Speaker.end();
    Serial.println("Whisper STT");
    String ret = SpeechToText();
    M5.Speaker.begin();
    if(ret != "") {
      set_led_color(CRGB::LightGreen);
      Serial.println(ret);
      if (!wav->isRunning()) {
        String response = exec_chatGPT(ret);
        if(response != "") {
          set_led_color(CRGB::Blue);
          //M5.Speaker.begin();
          Voicevox_tts((char*)response.c_str(), (char*)TTS_VOICE.c_str());
        }
      }
    } else {
      set_led_color(CRGB::Red);
      delay(2000);
      set_led_color(CRGB::Black);
    }
     M5.Speaker.begin();
  }

  if (wav->isRunning()) {
    if (!wav->loop()) {
      wav->stop();
      if(file != nullptr){delete file; file = nullptr;}
      if (buff != nullptr){delete buff; buff = nullptr;}
      Serial.println("wav stop");
      speech_text_buffer = "";
      set_led_color(CRGB::Black);
    }
    delay(1);
  }
}
