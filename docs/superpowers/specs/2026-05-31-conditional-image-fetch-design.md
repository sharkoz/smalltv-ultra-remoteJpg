# Conditional image fetch — design

**Date:** 2026-05-31

## Goal

Avoid unnecessary work (bandwidth, JPEG decode, screen redraw) when the remote
image hasn't changed since the last poll. Today `imageFetch()`
(`src/image.cpp`) does a full `GET`, writes the whole JPEG to LittleFS, decodes
and redraws on every `refreshInterval` tick — even when the image is identical.

The behavior is **always active by default**, with no user-facing toggle. It is
transparent and has no downside.

## Source assumption

The image source varies and is unknown (static file server, CDN, or dynamic
endpoint). The design must degrade gracefully when the server provides no
cache validators.

## Approach: conditional GET + content-hash fallback

A single `GET` request per poll (same request count as today), layered:

1. **Conditional request.** Send `If-None-Match` (last stored ETag) and/or
   `If-Modified-Since` (last stored Last-Modified) when we have them.
2. **Server supports validators** → responds `304 Not Modified`, no body
   downloaded → do nothing. Maximum saving (bandwidth + decode + redraw).
3. **Server without validators** → always responds `200` with the body. We
   download as today **but** compute a lightweight FNV-1a hash of the stream
   while writing. If the hash matches the previous one → **skip the JPEG decode
   and redraw**. Saves CPU and flicker, not bandwidth.

HEAD-based variants were rejected: they add a systematic extra request and rely
on `Content-Length`/`Last-Modified` being stable on HEAD, which is often false
on dynamic endpoints.

## State (RAM only, in `src/image.cpp`)

```cpp
static String   lastEtag;          // last ETag received
static String   lastModified;      // last Last-Modified received
static uint32_t lastHash = 0;      // FNV-1a hash of last downloaded body
static bool     haveHash = false;  // false until first successful fetch
```

RAM only — not persisted to `config.json`. After a reboot we do one full fetch
+ redraw, which is desirable anyway. No flash cost, no extra config key.

## Flow in `imageFetch()`

```
1. http.begin() + http.collectHeaders({"ETag", "Last-Modified"})
2. if lastEtag non-empty      → http.addHeader("If-None-Match", lastEtag)
   if lastModified non-empty  → http.addHeader("If-Modified-Since", lastModified)
3. code = http.GET()
   ├─ 304 Not Modified  → nothing to do, return true (image already on screen)
   ├─ != 200 && != 304  → log error, return false
   └─ 200 → stream body into /img.jpg as today,
            computing the FNV-1a hash byte-by-byte while writing
4. after download:
   ├─ total == 0                    → error, return false (cache untouched)
   ├─ haveHash && hash == lastHash  → skip decode+redraw,
   │                                  refresh etag/lastModified only, return true
   └─ else → TJpgDec.drawFsJpg(...)
             ├─ decode failure → return false, cache NOT updated (retry next time)
             └─ success → lastHash=hash; haveHash=true;
                          lastEtag=newEtag; lastModified=newLastMod; return true
```

**Robustness rule:** the cache (hash/ETag/Last-Modified) is updated **only after
a successful decode**. A truncated download (100KB cap hit) or corrupt body never
freezes the display — the next poll retries.

## FNV-1a hash

Lightweight, no dependency, folded into the existing read loop:

```cpp
uint32_t h = 2166136261u;          // init before the loop
// for each byte b read into buf[]: h = (h ^ b) * 16777619u;
```

No extra buffer — operates on the bytes already in `buf[]`.

## What does not change

- `refreshInterval` polling cadence is unchanged — we still poll every N seconds,
  we just skip the redundant work when nothing changed.
- Public API `imageFetch(const String& url)` is unchanged → no changes in
  `main.cpp`.

## Testing

This is firmware on ESP8266; no host test harness exists in the repo. Validation
is manual via serial monitor:

- Point at a static file (e.g. GitHub raw / S3): confirm `304` logged on second
  poll, no "bytes downloaded" line, no redraw.
- Point at a dynamic endpoint returning identical bytes with no validators:
  confirm "bytes downloaded" each poll but no redraw (hash match logged).
- Change the image: confirm a new download + redraw occurs.
- Truncated/corrupt response: confirm the cache is not poisoned (retries next
  poll).
