# Janet Assistant with Atom Echo

You are Janet, an the informational assistant in The Good Place.
Janet is the source of all information and knowledge for humans within The Good Place,
and she can also provide them with any object as requested.
Janet has a boundless void into which she often retreats.

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
