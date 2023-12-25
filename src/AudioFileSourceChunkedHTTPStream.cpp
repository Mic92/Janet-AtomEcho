/*
  AudioFileSourceHTTPStream
  Streaming HTTP source

  Copyright (C) 2017  Earle F. Philhower, III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "AudioFileSourceChunkedHTTPStream.h"

AudioFileSourceChunkedHTTPStream::AudioFileSourceChunkedHTTPStream()
{
  pos = 0;
  reconnectTries = 0;
  chunkSize = -1;
  saveURL[0] = 0;
}

AudioFileSourceChunkedHTTPStream::AudioFileSourceChunkedHTTPStream(const char *url, const std::string body): saveBody(body)
{
  saveURL[0] = 0;
  reconnectTries = 0;
  chunkSize = -1;
  open(url);
}

bool AudioFileSourceChunkedHTTPStream::open(const char *url)
{
  pos = 0;
  http.begin(client, url);
  http.setReuse(true);
#ifndef ESP32
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
#endif
  int code = http.POST((uint8_t*)saveBody.c_str(), saveBody.length());
  if (code != HTTP_CODE_OK) {
    http.end();
    cb.st(STATUS_HTTPFAIL, PSTR("Can't open HTTP request"));
    return false;
  }
  size = http.getSize();
  strncpy(saveURL, url, sizeof(saveURL));
  saveURL[sizeof(saveURL)-1] = 0;
  return true;
}

AudioFileSourceChunkedHTTPStream::~AudioFileSourceChunkedHTTPStream()
{
  http.end();
}

uint32_t AudioFileSourceChunkedHTTPStream::read(void *data, uint32_t len)
{
  if (data==NULL) {
    audioLogger->printf_P(PSTR("ERROR! AudioFileSourceChunkedHTTPStream::read passed NULL data\n"));
    return 0;
  }
  return readInternal(data, len, false);
}

uint32_t AudioFileSourceChunkedHTTPStream::readNonBlock(void *data, uint32_t len)
{
  if (data==NULL) {
    audioLogger->printf_P(PSTR("ERROR! AudioFileSourceChunkedHTTPStream::readNonBlock passed NULL data\n"));
    return 0;
  }
  return readInternal(data, len, true);
//  return readInternal(data, len, false);
}

uint32_t AudioFileSourceChunkedHTTPStream::readInternal(void *data, uint32_t len, bool nonBlock)
{
retry:
  if (!http.connected()) {
    cb.st(STATUS_DISCONNECTED, PSTR("Stream disconnected"));
    http.end();
    for (int i = 0; i < reconnectTries; i++) {
      char buff[64];
      sprintf_P(buff, PSTR("Attempting to reconnect, try %d"), i);
      cb.st(STATUS_RECONNECTING, buff);
      delay(reconnectDelayMs);
      if (open(saveURL)) {
        cb.st(STATUS_RECONNECTED, PSTR("Stream reconnected"));
        break;
      }
    }
    if (!http.connected()) {
      cb.st(STATUS_DISCONNECTED, PSTR("Unable to reconnect"));
      return 0;
    }
  }
  if ((size > 0) && (pos >= size)) return 0;

  WiFiClient *stream = http.getStreamPtr();

  // Can't read past EOF...
  if ( (size > 0) && (len > (uint32_t)(pos - size)) ) len = pos - size;

  if (!nonBlock) {
    int start = millis();
    while ((stream->available() < (int)len) && (millis() - start < 500)) yield();
  }

  size_t avail = stream->available();
  if (!nonBlock && !avail) {
    cb.st(STATUS_NODATA, PSTR("No stream data available"));
    http.end();
    goto retry;
  }
  if (avail == 0) return 0;
  if (avail < len) len = avail;
  int totalRead = 0;

  while (len > 0) {
     if (chunkSize < 0) {
       // Read the chunk size
       char buff[16];
       int read = stream->readBytesUntil('\n', buff, sizeof(buff));
       if (read <= 0) {
         cb.st(STATUS_NODATA, PSTR("No chunk size data available"));
         http.end();
         goto retry;
       }
       buff[read-1] = 0;
       chunkSize = strtol(buff, NULL, 16);
       if (chunkSize == 0) {
         // End of stream
         http.end();
         return 0;
       }
     }

     int maxLen = chunkSize > len ? len : chunkSize;
     int read = stream->read(reinterpret_cast<uint8_t*>(data), maxLen);
     pos += read;
     data = reinterpret_cast<uint8_t*>(data) + read;
     chunkSize -= read;
     len -= read;
     totalRead += read;

     if (chunkSize == 0) {
       // Read the trailing \r\n
       char buff[2];
       stream->readBytes(buff, 2);
       if (buff[0] != '\r' || buff[1] != '\n') {
         audioLogger->printf_P(PSTR("ERROR! AudioFileSourceChunkedHTTPStream::readInternal failed to read trailing \\r\\n\n"));
         return 0;
       }
       chunkSize = -1;
     }
  }

  return totalRead;
}

bool AudioFileSourceChunkedHTTPStream::seek(int32_t pos, int dir)
{
  audioLogger->printf_P(PSTR("ERROR! AudioFileSourceChunkedHTTPStream::seek not implemented!"));
  (void) pos;
  (void) dir;
  return false;
}

bool AudioFileSourceChunkedHTTPStream::close()
{
  http.end();
  return true;
}

bool AudioFileSourceChunkedHTTPStream::isOpen()
{
  return http.connected();
}

uint32_t AudioFileSourceChunkedHTTPStream::getSize()
{
  return size;
}

uint32_t AudioFileSourceChunkedHTTPStream::getPos()
{
  return pos;
}
