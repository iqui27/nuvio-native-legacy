"""Deterministic fake Emby 4.8 for tests/emby.sh.

Serves under a reverse-proxy prefix (/media) AND the /emby API root, like a real
Emby behind a proxy: /media/emby/<route>. Rejects the Jellyfin-only routes
(/UserViews, /Items?userId=, /QuickConnect/*) so a request built the Jellyfin
way fails the test. /jf/emby/System/Info/Public answers as a Jellyfin server to
prove the product check. Never prints headers, tokens, passwords or payloads.
"""
import json
import re
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

SERVER_ID = "9f8e7d6c5b4a39281706f5e4d3c2b1a0"
USER_ID = "0a1b2c3d4e5f60718293a4b5c6d7e8f9"
LIB_MOVIES = "11"
LIB_SHOWS = "22"
PASSWORD = "emby pässword"
TOKEN = "embytoken0123456789abcdef0123456789"

lock = threading.Lock()
state = {"events": [], "unknown": []}


def movie(i):
    return {"Name": "Emby Movie %d" % i, "Id": str(1000 + i), "Type": "Movie", "ProductionYear": 2010 + i,
            "RunTimeTicks": 72000000000, "Overview": "Overview %d" % i, "Genres": ["Action"],
            "OfficialRating": "PG", "CommunityRating": 6.5,
            "ImageTags": {"Primary": "pt%d" % i}, "BackdropImageTags": ["bt%d" % i],
            "UserData": {"PlaybackPositionTicks": 18000000000, "PlayedPercentage": 25.0, "Played": False}}


SERIES = {"Name": "Emby Show", "Id": "2001", "Type": "Series", "ProductionYear": 2019, "Overview": "Show",
          "ImageTags": {"Primary": "sp"}, "People": [{"Name": "An Actor", "Id": "77", "Type": "Actor",
                                                       "Role": "Lead", "PrimaryImageTag": "ap"},
                                                      {"Name": "A Director", "Id": "78", "Type": "Director"}]}


def episodes():
    out = []
    for s in (1, 2):
        for e in (1, 2):
            out.append({"Name": "Ep %d-%d" % (s, e), "Id": str(3000 + s * 10 + e), "Type": "Episode",
                        "ParentIndexNumber": s, "IndexNumber": e, "RunTimeTicks": 27000000000,
                        "ImageTags": {"Primary": "ep"}})
    return out


def media_source(item):
    return {"Id": "mediasource_%s" % item, "Container": "mkv", "Name": "Emby 1080p", "Size": 4000000000,
            "SupportsDirectPlay": True, "SupportsDirectStream": True, "SupportsTranscoding": True,
            "Path": "/srv/private/movie.mkv",
            "TranscodingUrl": "/videos/%s/master.m3u8?MediaSourceId=mediasource_%s&PlaySessionId=ps1&api_key=%s"
                              % (item, item, TOKEN),
            "MediaStreams": [{"Type": "Video", "Codec": "hevc", "Height": 1080, "VideoRange": "HDR"},
                             {"Type": "Audio", "Codec": "eac3", "Channels": 6}]}


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        self.dispatch("GET")

    def do_POST(self):
        self.dispatch("POST")

    def do_DELETE(self):
        self.dispatch("DELETE")

    def reply(self, status, obj=None, raw=None):
        body = raw if raw is not None else (b"" if obj is None else json.dumps(obj).encode())
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body:
            self.wfile.write(body)

    def hdr_auth(self):
        h = self.headers.get("X-Emby-Authorization", "")
        if not h.startswith("MediaBrowser ") or 'Client="Nuvio"' not in h:
            return None
        m = re.search(r'Token="([^"]*)"', h)
        return m.group(1) if m else ""

    def authed(self):
        t = self.hdr_auth()
        return t == TOKEN and self.headers.get("X-Emby-Token", "") == TOKEN and \
            not self.headers.get("Authorization")

    def body(self):
        n = int(self.headers.get("Content-Length", "0") or 0)
        return json.loads(self.rfile.read(n) or b"{}") if n else {}

    def dispatch(self, method):
        u = urlparse(self.path)
        q = {k.lower(): v for k, v in parse_qs(u.query).items()}
        p = u.path
        if p.startswith("/control/"):
            with lock:
                if p == "/control/events":
                    return self.reply(200, state["events"])
                if p == "/control/unknown":
                    return self.reply(200, state["unknown"])
            return self.reply(404)
        if p == "/jf/emby/System/Info/Public":
            return self.reply(200, {"ServerName": "Actually Jellyfin", "Version": "10.10.3",
                                    "ProductName": "Jellyfin Server", "Id": SERVER_ID})
        if not p.startswith("/media/emby/"):
            return self.reply(404, {"error": "not here"})
        p = p[len("/media/emby"):]
        if p == "/System/Info/Public" and method == "GET":
            if self.hdr_auth() is None:
                return self.reply(400)
            return self.reply(200, {"ServerName": "Emby Home", "Version": "4.8.8.0", "Id": SERVER_ID,
                                    "LocalAddress": "http://10.0.0.3:8096", "WanAddress": ""})
        if p == "/Users/AuthenticateByName" and method == "POST":
            if self.hdr_auth() is None:
                return self.reply(400)
            b = self.body()
            if b.get("Username") != "emby user" or b.get("Pw") != PASSWORD:
                return self.reply(401, {"error": "bad"})
            return self.reply(200, {"AccessToken": TOKEN, "ServerId": SERVER_ID,
                                    "User": {"Id": USER_ID, "Name": "emby user"}})
        if not self.authed():
            return self.reply(401)
        base = "/Users/%s" % USER_ID
        if p == base + "/Views" and method == "GET":
            return self.reply(200, {"Items": [
                {"Name": "Movies", "Id": LIB_MOVIES, "CollectionType": "movies"},
                {"Name": "Shows", "Id": LIB_SHOWS, "CollectionType": "tvshows"},
                {"Name": "Music", "Id": "33", "CollectionType": "music"}]})
        if p == base + "/Items" and method == "GET":
            parent = q.get("parentid", [""])[0]
            items = [movie(1), movie(2)] if parent == LIB_MOVIES else [SERIES] if parent == LIB_SHOWS else []
            if not parent and q.get("recursive", [""])[0] == "true":
                # The matching index: Emby spells the keys like Jellyfin.
                m2 = movie(2)
                m2["ProviderIds"] = {"Imdb": "tt0002002"}
                m2["DateLastSaved"] = "2026-08-01T10:00:00.0000000Z"
                sh = dict(SERIES)
                sh["ProviderIds"] = {"Tmdb": "4242"}
                items = [movie(1), m2, sh]
            start = int(q.get("startindex", ["0"])[0])
            return self.reply(200, {"Items": items[start:], "TotalRecordCount": len(items)})
        m = re.fullmatch(re.escape(base) + r"/Items/(\d+)", p)
        if m and method == "GET":
            i = m.group(1)
            if i == "2001":
                return self.reply(200, SERIES)
            if i in ("1001", "1002"):
                d = movie(int(i) - 1000)
                d["People"] = [{"Name": "Mov Actor", "Id": "88", "Type": "Actor", "Role": "R"}]
                return self.reply(200, d)
            return self.reply(404)
        if p == "/Shows/2001/Episodes" and method == "GET":
            if q.get("userid", [""])[0] != USER_ID:
                return self.reply(400)
            return self.reply(200, {"Items": episodes(), "TotalRecordCount": 4})
        m = re.fullmatch(r"/Items/(\d+)/PlaybackInfo", p)
        if m and method == "POST":
            b = self.body()
            if b.get("UserId") != USER_ID or "DeviceProfile" not in b:
                return self.reply(400)
            return self.reply(200, {"PlaySessionId": "ps1", "MediaSources": [media_source(m.group(1))]})
        if p in ("/Sessions/Playing", "/Sessions/Playing/Progress", "/Sessions/Playing/Stopped") and method == "POST":
            b = self.body()
            with lock:
                state["events"].append({"route": p.rsplit("/", 1)[1], "ticks": b.get("PositionTicks"),
                                        "source": b.get("MediaSourceId"), "item": b.get("ItemId"),
                                        "session": b.get("PlaySessionId")})
            return self.reply(204)
        if p == "/Videos/ActiveEncodings" and method == "DELETE":
            with lock:
                state["events"].append({"route": "encoding-delete", "session": q.get("playsessionid", [""])[0]})
            return self.reply(204)
        if p == "/Sessions/Logout" and method == "POST":
            with lock:
                state["events"].append({"route": "logout"})
            return self.reply(204)
        with lock:
            state["unknown"].append(method + " " + p)
        return self.reply(404)


def main():
    srv = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    open(sys.argv[1], "w").write("%d\n" % srv.server_address[1])
    srv.serve_forever()


main()
