"""Deterministic fake plex.tv + Plex Media Server for tests/plex.sh.

One process plays both: /api/v2/* is plex.tv (PIN, resources, user), every
other path is the PMS. Never prints headers, tokens or payloads. /control/* is
for the test only.
"""
import json
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

MACHINE = "0123456789abcdef0123456789abcdef01234567"
ACCT_TOKEN = "plexacct-token-0001"
SRV_TOKEN = "plexsrv-token-0001"
PIN_ID = 123456
PORT = [0]

lock = threading.Lock()
state = {"polls": 0, "total_polls": 0, "events": [], "pins": 0, "sections_hits": 0, "probe_hits": 0,
         "decisions": 0, "refuse": set()}

MOVIES = [
    {"ratingKey": "101", "type": "movie", "title": "Movie One", "year": 1999, "summary": "Overview one",
     "rating": 7.8, "contentRating": "R", "duration": 7200000, "viewOffset": 3600000,
     "thumb": "/library/metadata/101/thumb/17", "art": "/library/metadata/101/art/17",
     "Genre": [{"tag": "Drama"}, {"tag": "Thriller"}],
     "Guid": [{"id": "imdb://tt0000101"}, {"id": "tmdb://5101"}, {"id": "tvdb://301"}]},
    {"ratingKey": "102", "type": "movie", "title": "Movie Two", "year": 2005, "duration": 5400000,
     "thumb": "/library/metadata/102/thumb/17",
     "Guid": [{"id": "tmdb://5102"}]},
    {"ratingKey": "103", "type": "movie", "title": "No External Id", "year": 2010, "duration": 5400000,
     "Guid": []},
]
SHOWS = [
    {"ratingKey": "201", "type": "show", "title": "Show One", "year": 2020, "summary": "Show overview",
     "thumb": "/library/metadata/201/thumb/17",
     "Genre": [{"tag": "Comedy"}],
     "Guid": [{"id": "imdb://tt0000201"}, {"id": "tmdb://7201"}]},
]
EPISODES = [
    {"ratingKey": "301", "type": "episode", "parentIndex": 1, "index": 1, "title": "Pilot", "duration": 2700000,
     "thumb": "/library/metadata/301/thumb/17", "originallyAvailableAt": "2020-01-02"},
    {"ratingKey": "302", "type": "episode", "parentIndex": 1, "index": 2, "title": "Second", "duration": 2700000},
    {"ratingKey": "311", "type": "episode", "parentIndex": 2, "index": 1, "title": "Return", "duration": 2700000},
]


def media_for(rk):
    return {"ratingKey": rk, "duration": 7200000, "Media": [
        {"id": 501, "duration": 7200000, "bitrate": 8000, "width": 1920, "height": 1080, "audioChannels": 6,
         "audioCodec": "eac3", "videoCodec": "hevc", "videoResolution": "1080", "container": "mkv",
         "Part": [{"id": 901, "key": "/library/parts/901/1700000000/file.mkv", "duration": 7200000,
                   "file": "/srv/private/Movie.mkv", "size": 12345678901, "container": "mkv",
                   "Stream": [{"id": 1, "streamType": 1, "codec": "hevc", "colorTrc": "smpte2084",
                               "DOVIPresent": False},
                              {"id": 2, "streamType": 2, "codec": "eac3", "channels": 6}]}]},
        {"id": 502, "duration": 7200000, "width": 1280, "height": 720, "audioChannels": 2,
         "audioCodec": "aac", "videoCodec": "h264", "videoResolution": "720", "container": "mp4",
         "Part": [{"id": 902, "key": "/library/parts/902/1700000001/file.mp4", "size": 999,
                   "container": "mp4"}]},
        # DTS: no TV backend here opens it; never offered as direct play.
        {"id": 503, "duration": 7200000, "width": 3840, "height": 2160, "audioChannels": 8,
         "audioCodec": "dca", "videoCodec": "hevc", "videoResolution": "4k", "container": "mkv",
         "Part": [{"id": 903, "key": "/library/parts/903/1700000002/file.mkv", "size": 4242,
                   "container": "mkv",
                   "Stream": [{"id": 5, "streamType": 1, "codec": "hevc"},
                              {"id": 6, "streamType": 2, "codec": "dca", "channels": 8, "selected": True}]}]},
    ]}


def container(items, total=None):
    return {"MediaContainer": {"size": len(items), "totalSize": total if total is not None else len(items),
                               "Metadata": items}}


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        self.dispatch("GET")

    def do_POST(self):
        self.dispatch("POST")

    def reply(self, status, obj=None):
        body = b"" if obj is None else json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body:
            self.wfile.write(body)

    def tok(self):
        return self.headers.get("X-Plex-Token", "")

    def dispatch(self, method):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        p = u.path
        if p.startswith("/control/"):
            if p == "/control/events":
                with lock:
                    return self.reply(200, state["events"])
            if p == "/control/counts":
                with lock:
                    return self.reply(200, {"probe": state["probe_hits"], "pins": state["pins"],
                                            "sections": state["sections_hits"],
                                            "polls": state["total_polls"],
                                            "decisions": state["decisions"]})
            if p == "/control/refuse":
                with lock:
                    state["refuse"] = set(int(x) for x in q.get("media", [""])[0].split(",") if x)
                return self.reply(200, {})
            return self.reply(404)
        cid = self.headers.get("X-Plex-Client-Identifier", "")
        if p.startswith("/api/v2/"):
            if not cid or self.headers.get("Accept") != "application/json":
                return self.reply(400, {"errors": [{"code": 1000, "message": "headers"}]})
            if p == "/api/v2/pins" and method == "POST":
                with lock:
                    state["polls"] = 0
                    state["pins"] += 1
                return self.reply(201, {"id": PIN_ID, "code": "WXYZ", "product": "Nuvio", "trusted": False,
                                        "clientIdentifier": cid, "expiresIn": 900, "authToken": None})
            if p == "/api/v2/pins/%d" % PIN_ID and method == "GET":
                with lock:
                    state["polls"] += 1
                    state["total_polls"] += 1
                    linked = state["polls"] >= 2
                return self.reply(200, {"id": PIN_ID, "code": "WXYZ", "expiresIn": 800,
                                        "authToken": ACCT_TOKEN if linked else None})
            if p.startswith("/api/v2/pins/"):
                return self.reply(404, {"errors": [{"code": 1020, "message": "Code not found or expired"}]})
            if self.tok() != ACCT_TOKEN:
                return self.reply(401, {"errors": [{"code": 1001, "message": "denied"}]})
            if p == "/api/v2/user":
                return self.reply(200, {"id": 7, "username": "someone", "title": "Some One",
                                        "email": "private@example.invalid"})
            if p == "/api/v2/resources":
                port = PORT[0]
                return self.reply(200, [
                    {"name": "Phone", "product": "Plex for Android", "provides": "player",
                     "clientIdentifier": "ffff", "connections": []},
                    {"name": "Home PMS", "product": "Plex Media Server", "provides": "server",
                     "clientIdentifier": MACHINE, "productVersion": "1.41.0", "owned": True,
                     "accessToken": SRV_TOKEN, "connections": [
                         {"protocol": "https", "address": "10-9-9-9.deadbeef.plex.direct", "port": 32400,
                          "uri": "https://10-9-9-9.deadbeef.plex.direct:32400", "local": False, "relay": True,
                          "IPv6": False},
                         {"protocol": "http", "address": "127.0.0.1", "port": port,
                          "uri": "http://127.0.0.1:%d" % port, "local": True, "relay": False, "IPv6": False},
                     ]},
                ])
            return self.reply(404)
        # ---- the PMS
        if p == "/identity":
            with lock:
                state["probe_hits"] += 1
            return self.reply(200, {"MediaContainer": {"size": 0, "machineIdentifier": MACHINE,
                                                       "version": "1.41.0.8992"}})
        if self.tok() != SRV_TOKEN:
            return self.reply(401)
        if p == "/library/sections":
            with lock:
                state["sections_hits"] += 1
            return self.reply(200, {"MediaContainer": {"size": 3, "Directory": [
                {"key": "1", "type": "movie", "title": "Movies"},
                {"key": "2", "type": "show", "title": "TV Shows"},
                {"key": "3", "type": "artist", "title": "Music"}]}})
        if p == "/library/sections/1/all":
            return self.reply(200, self.page(MOVIES, q))
        if p == "/library/sections/2/all":
            return self.reply(200, self.page(SHOWS, q))
        if p == "/library/metadata/201/allLeaves":
            return self.reply(200, container(EPISODES))
        if p.startswith("/library/metadata/"):
            rk = p.rsplit("/", 1)[1]
            for it in MOVIES + SHOWS + EPISODES:
                if it["ratingKey"] == rk:
                    d = dict(it)
                    if it["type"] in ("movie", "episode"):
                        d.update(media_for(rk))
                    d["Role"] = [{"tag": "Actor One", "role": "Lead", "thumb": "https://image.example/a.jpg"},
                                 {"tag": "Actor Two", "role": "Support", "thumb": "/library/metadata/9/thumb/1"}]
                    d["Director"] = [{"tag": "Dir One"}]
                    return self.reply(200, container([d]))
            return self.reply(404)
        if p == "/:/timeline":
            with lock:
                state["events"].append({"state": q.get("state", [""])[0], "time": q.get("time", [""])[0],
                                        "rk": q.get("ratingKey", [""])[0], "dur": q.get("duration", [""])[0]})
            return self.reply(200, {})
        if p == "/video/:/transcode/universal/decision":
            # The profile must come in the header, and the item by path.
            extra = self.headers.get("X-Plex-Client-Profile-Extra", "")
            if "add-direct-play-profile" not in extra or "dca" in extra or "truehd" in extra:
                return self.reply(400)
            if not q.get("path", [""])[0].startswith("/library/metadata/"):
                return self.reply(400)
            mi = int(q.get("mediaIndex", ["0"])[0])
            with lock:
                state["decisions"] += 1
                refused = mi in state["refuse"]
            code = 3000 if refused else 1000
            return self.reply(200, {"MediaContainer": {
                "size": 1, "directPlayDecisionCode": code,
                "directPlayDecisionText": "App cannot direct play this item." if refused else "Direct play OK.",
                "Metadata": [{"Media": [{"Part": [{"decision": "transcode" if refused else "directplay"}]}]}]}})
        if p == "/video/:/transcode/universal/stop":
            with lock:
                state["events"].append({"state": "transcode-stop", "session": q.get("session", [""])[0]})
            return self.reply(200, {})
        return self.reply(404)

    def page(self, items, q):
        start = int(q.get("X-Plex-Container-Start", ["0"])[0])
        size = int(q.get("X-Plex-Container-Size", ["50"])[0])
        return container(items[start:start + size], total=len(items))


def main():
    srv = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    PORT[0] = srv.server_address[1]
    open(sys.argv[1], "w").write("%d\n" % PORT[0])
    srv.serve_forever()


main()
