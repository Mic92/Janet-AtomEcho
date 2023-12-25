# ChatGPT bot with Echo

## Dependencies

- [platformio](https://platformio.org)
- OpenAI api key
- server-piper-tts: https://github.com/arunk140/serve-piper-tts

## Configuration & flashing

```
cp src/config.h.example ./src/config.h
vim ./src/config.h
platformio run --target upload
```

Also needs piper for text-to-speech:

```
$ docker run -p 8081:8080 ghcr.io/arunk140/serve-piper-tts:latest
```
