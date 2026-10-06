"""Deterministic fake Jellyfin for tests/jellyfin.sh: 10.10 by default,
/control/version?v=12.0.0 (or 10.7.5) switches how the query token is read.

Serves under a reverse-proxy prefix (/jf) so base paths are exercised. Never
prints headers, tokens, passwords or payloads. Control routes (/control/*)
are for the test only.
"""
import json
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

SERVER_ID = "0f1e2d3c4b5a69788796a5b4c3d2e1f0"
USER_ID = "aa11bb22cc33dd44ee55ff6677889900"
LIB_MOVIES = "1111aaaa2222bbbb3333cccc4444dddd"
LIB_SHOWS = "5555eeee6666ffff7777aaaa8888bbbb"
LIB_MUSIC = "9999cccc0000dddd1111eeee2222ffff"
SERIES = "abcdef0123456789abcdef0123456789"      # no external id: stays opaque
SERIES2 = "5e1e5e1e5e1e5e1e5e1e5e1e5e1e5e1e"     # IMDb + TVDB: matched
PASSWORD = "pässwörd \"quoted\""

lock = threading.Lock()
state = {
    "tokens": set(),
    "events": [],
    "polls": 0,
    "slow": 0.0,
    "secret": "QcSecret0123456789",
    "version": "10.10.3",
    "extra": [],            # movies added after the first index pass
    "gone": set(),          # movie numbers deleted on the server
    "index_calls": [],      # (StartIndex, Limit, MinDateLastSaved) of index requests
    "episode_index": 0,
}


def ver_tuple():
    try:
        return tuple(int(x) for x in state["version"].split(".")[:2])
    except ValueError:
        return (0, 0)


def movie_id(i):
    return "%032x" % (0xC0FFEE000 + i)


def episode_id(s, e):
    return "%032x" % (0xE9000000 + s * 100 + e)


def saved(i):
    return "2026-09-01T%02d:%02d:00.0000000Z" % (i // 60, i % 60)


def ids(i):
    # 0..4 have no external id (home videos); 5..49 IMDb+TMDB; 50..59 TMDB only.
    if i < 5:
        return {}
    if i < 50:
        return {"Imdb": "tt%07d" % (1000 + i), "Tmdb": str(5000 + i)}
    return {"Tmdb": str(5000 + i)}


def movie(i):
    mid = movie_id(i)
    return {
        "ProviderIds": ids(i), "DateLastSaved": saved(i), "Height": 2160 if i == 7 else 1080,
        "Name": "Movie %02d" % i,
        "Id": mid,
        "ServerId": SERVER_ID,
        "Type": "Movie",
        "ProductionYear": 2000 + i,
        "RunTimeTicks": 72000000000,
        "Overview": "Overview %d" % i,
        "Genres": ["Drama", "Mystery"],
        "OfficialRating": "PG-13",
        "CommunityRating": 7.4,
        "ImageTags": {"Primary": "ptag%d" % i, "Logo": "ltag"},
        "BackdropImageTags": ["btag%d" % i],
        "UserData": {"PlaybackPositionTicks": 36000000000, "PlayedPercentage": 50.0,
                     "Played": False, "Key": mid},
    }


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

    def auth(self):
        h = self.headers.get("Authorization", "")
        if not h.startswith("MediaBrowser "):
            return None, None
        dev = re.search(r'DeviceId="([^"]*)"', h)
        tok = re.search(r'Token="([^"]*)"', h)
        return (dev.group(1) if dev else None), (tok.group(1) if tok else None)

    def authed(self):
        _, tok = self.auth()
        with lock:
            return tok in state["tokens"]

    def query_token(self, q):
        """Token from the query, the way each server version reads it.

        12.0 turned legacy auth off: "api_key" is ignored there. 10.8+ reads
        "ApiKey"; older servers only "api_key"."""
        v = ver_tuple()
        if "ApiKey" in q and v >= (10, 8):
            return q["ApiKey"][0]
        if "api_key" in q and v < (12, 0):
            return q["api_key"][0]
        return None

    def body(self):
        n = int(self.headers.get("Content-Length", "0") or 0)
        return json.loads(self.rfile.read(n) or b"{}") if n else {}

    def dispatch(self, method):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        p = u.path
        if p.startswith("/control/"):
            return self.control(p, q)
        if not p.startswith("/jf/"):
            return self.reply(404, {"error": "not here"})
        p = p[3:]
        dev, tok = self.auth()
        if p == "/System/Info/Public" and method == "GET":
            return self.reply(200, {"LocalAddress": "http://10.0.0.2:8096", "ServerName": "Home Server",
                                    "Version": state["version"], "ProductName": "Jellyfin Server",
                                    "Id": SERVER_ID, "StartupWizardCompleted": True})
        if p == "/QuickConnect/Enabled" and method == "GET":
            return self.reply(200, raw=b"true")
        if p == "/QuickConnect/Initiate":
            if method != "POST":
                return self.reply(405)
            if not dev:
                return self.reply(400)
            with lock:
                state["polls"] = 0
            return self.reply(200, {"Authenticated": False, "Secret": state["secret"], "Code": "482913",
                                    "DeviceId": dev, "AppName": "Nuvio"})
        if p == "/QuickConnect/Connect" and method == "GET":
            if q.get("secret", [""])[0] != state["secret"]:
                return self.reply(404)
            with lock:
                state["polls"] += 1
                ok = state["polls"] >= 2
            return self.reply(200, {"Authenticated": ok, "Secret": state["secret"], "Code": "482913"})
        if p == "/Users/AuthenticateWithQuickConnect" and method == "POST":
            b = self.body()
            with lock:
                ok = b.get("Secret") == state["secret"] and state["polls"] >= 2
                if ok:
                    state["tokens"].add("qctoken000000000000000000000001")
            if not ok:
                return self.reply(401)
            return self.reply(200, {"User": {"Name": "ana", "Id": USER_ID, "ServerId": SERVER_ID},
                                    "SessionInfo": {"Id": "s1", "UserId": USER_ID},
                                    "AccessToken": "qctoken000000000000000000000001", "ServerId": SERVER_ID})
        if p == "/Users/AuthenticateByName" and method == "POST":
            b = self.body()
            if b.get("Username") != "ana" or b.get("Pw") != PASSWORD:
                return self.reply(401)
            with lock:
                state["tokens"].add("pwtoken0000000000000000000000002")
            return self.reply(200, {"User": {"Name": "ana", "Id": USER_ID, "ServerId": SERVER_ID,
                                             "Policy": {"Id": "nested-should-not-win"}},
                                    "AccessToken": "pwtoken0000000000000000000000002", "ServerId": SERVER_ID})
        m = re.match(r"^/(?:Videos|videos)/([0-9a-f]{32})/(stream|master\.m3u8)$", p)
        if m and method == "GET":
            qt = self.query_token(q)
            with lock:
                ok = tok in state["tokens"] or qt in state["tokens"]
                state["events"].append({"op": "media", "ok": ok, "route": m.group(2)})
            return self.reply(200 if ok else 401, {"media": m.group(2)})
        if not self.authed():
            return self.reply(401)
        if p == "/Sessions/Logout" and method == "POST":
            with lock:
                state["tokens"].discard(tok)
                state["events"].append({"op": "logout"})
            return self.reply(204)
        if p == "/UserViews":
            if q.get("userId", [""])[0] != USER_ID:
                return self.reply(400)
            return self.reply(200, {"Items": [
                {"Name": "Movies", "Id": LIB_MOVIES, "CollectionType": "movies"},
                {"Name": "Shows", "Id": LIB_SHOWS, "CollectionType": "tvshows"},
                {"Name": "Music", "Id": LIB_MUSIC, "CollectionType": "music"}],
                "TotalRecordCount": 3})
        if p == "/Items" and method == "GET":
            parent = q.get("ParentId", [""])[0]
            start = int(q.get("StartIndex", ["0"])[0])
            limit = int(q.get("Limit", ["100"])[0])
            shows = [{"Name": "Show", "Id": SERIES, "Type": "Series", "ProductionYear": 2019,
                      "ImageTags": {"Primary": "sp"}, "BackdropImageTags": [],
                      "UserData": {"PlayedPercentage": 0}, "DateLastSaved": saved(100)},
                     {"Name": "The Matched Show", "Id": SERIES2, "Type": "Series", "ProductionYear": 2021,
                      "ProviderIds": {"IMDB": "tt0900001", "Tvdb": "999"}, "DateLastSaved": saved(101),
                      "ImageTags": {"Primary": "s2"}, "UserData": {"PlayedPercentage": 0}}]
            with lock:
                movies = [movie(i) for i in range(60) if i not in state["gone"]] + list(state["extra"])
            if parent == LIB_MOVIES:
                allitems = movies
            elif parent == LIB_SHOWS:
                allitems = shows + [{"Name": "Ignored episode", "Id": episode_id(1, 1), "Type": "Episode"}]
            elif not parent and q.get("Recursive", [""])[0] == "true":
                # The matching index: every movie and series of the user.
                since = q.get("MinDateLastSaved", [""])[0]
                allitems = movies + shows
                if since:
                    allitems = [x for x in allitems if x.get("DateLastSaved", "")[:19] >= since[:19]]
                with lock:
                    state["index_calls"].append([start, limit, since])
            else:
                allitems = []
            return self.reply(200, {"Items": allitems[start:start + limit], "TotalRecordCount": len(allitems),
                                    "StartIndex": start})
        m = re.match(r"^/Items/([0-9a-f]{32})$", p)
        if m and method == "GET":
            iid = m.group(1)
            if iid == SERIES:
                obj = {"Name": "Show", "Id": SERIES, "Type": "Series", "ProductionYear": 2019,
                       "Overview": "Show overview", "ImageTags": {"Primary": "sp"}}
            else:
                obj = movie(1)
                obj["Id"] = iid
            obj["People"] = [
                {"Name": "Actor One", "Id": "%032x" % 1, "Role": "Lead", "Type": "Actor", "PrimaryImageTag": "a1"},
                {"Name": "Actor Two", "Id": "%032x" % 2, "Role": "Friend", "Type": "Actor"},
                {"Name": "Dir Ector", "Id": "%032x" % 3, "Type": "Director"}]
            obj["MediaSources"] = [{"Id": "ffffffffffffffffffffffffffffffff", "Name": "nested name",
                                    "RunTimeTicks": 1}]
            return self.reply(200, obj)
        m = re.match(r"^/Shows/([0-9a-f]{32})/Episodes$", p)
        if m:
            eps = []
            if "ProviderIds" in q.get("Fields", [""])[0]:
                with lock:
                    state["episode_index"] += 1
            for s in (1, 2):
                for e in (1, 2, 3):
                    eps.append({"Name": "S%dE%d" % (s, e), "Id": episode_id(s, e), "Type": "Episode",
                                "ParentIndexNumber": s, "IndexNumber": e, "RunTimeTicks": 26400000000,
                                "SeriesId": m.group(1),
                                "PremiereDate": "2019-0%d-1%dT00:00:00.0000000Z" % (s, e),
                                "Overview": "ep", "ImageTags": {"Primary": "et"}})
            return self.reply(200, {"Items": eps, "TotalRecordCount": len(eps)})
        m = re.match(r"^/Items/([0-9a-f]{32})/PlaybackInfo$", p)
        if m and method == "POST":
            with lock:
                slow = state["slow"]
                state["slow"] = 0.0
            if slow:
                time.sleep(slow)
            b = self.body()
            prof = b.get("DeviceProfile") or {}
            if not prof.get("DirectPlayProfiles") or b.get("StartTimeTicks") != 0:
                return self.reply(400)
            iid = m.group(1)
            with lock:
                state["events"].append({"op": "playbackinfo", "profile": prof.get("Name")})
            return self.reply(200, {"PlaySessionId": "ps0001", "MediaSources": [
                {"Protocol": "File", "Id": iid, "Path": "/srv/private/path/film.mkv", "Container": "mkv",
                 "Size": 21474836480, "Name": "Film - 2160p", "SupportsDirectPlay": True,
                 "SupportsDirectStream": True, "SupportsTranscoding": True,
                 "MediaStreams": [{"Codec": "hevc", "Type": "Video", "Height": 2160, "VideoRangeType": "HDR10"},
                                  {"Codec": "eac3", "Type": "Audio", "Channels": 6}]},
                {"Protocol": "File", "Id": "%032x" % 77, "Container": "avi", "Name": "Film - old",
                 "SupportsDirectPlay": False, "SupportsDirectStream": False, "SupportsTranscoding": True,
                 # 10.x servers embed the legacy name; 12.x embeds no token here.
                 "TranscodingUrl": "/videos/%s/master.m3u8?MediaSourceId=%032x&PlaySessionId=ps0001%s" % (
                     iid, 77, "" if ver_tuple() >= (12, 0) else "&api_key=" + (tok or "")),
                 "TranscodingSubProtocol": "hls",
                 "MediaStreams": [{"Codec": "mpeg4", "Type": "Video", "Height": 480},
                                  {"Codec": "mp3", "Type": "Audio", "Channels": 2}]}]})
        if p in ("/Sessions/Playing", "/Sessions/Playing/Progress", "/Sessions/Playing/Stopped") and method == "POST":
            b = self.body()
            with lock:
                state["events"].append({"op": p.rsplit("/", 1)[-1], "ticks": b.get("PositionTicks"),
                                        "paused": b.get("IsPaused"), "session": b.get("PlaySessionId"),
                                        "method": b.get("PlayMethod"), "event": b.get("EventName")})
            return self.reply(204)
        if p == "/Videos/ActiveEncodings" and method == "DELETE":
            with lock:
                state["events"].append({"op": "release", "session": q.get("playSessionId", [""])[0]})
            return self.reply(204)
        return self.reply(404)

    def control(self, p, q):
        with lock:
            if p == "/control/events":
                body = json.dumps(state["events"]).encode()
            elif p == "/control/reset":
                state["events"] = []
                body = b"{}"
            elif p == "/control/revoke":
                state["tokens"].clear()
                body = b"{}"
            elif p == "/control/index":
                body = json.dumps({"calls": state["index_calls"], "episodes": state["episode_index"]}).encode()
                state["index_calls"] = []
                state["episode_index"] = 0
            elif p == "/control/add":
                i = int(q.get("n", ["70"])[0])
                m = movie(i)
                m["DateLastSaved"] = "2026-09-30T00:00:00.0000000Z"
                m["ProviderIds"] = {"Imdb": "tt%07d" % (1000 + i)}
                state["extra"].append(m)
                body = b"{}"
            elif p == "/control/delete":
                state["gone"].add(int(q.get("n", ["5"])[0]))
                body = b"{}"
            elif p == "/control/version":
                state["version"] = q.get("v", ["10.10.3"])[0]
                body = b"{}"
            elif p == "/control/slow":
                state["slow"] = float(q.get("s", ["4"])[0])
                body = b"{}"
            else:
                body = b"{}"
        self.reply(200, raw=body)


class Server(ThreadingHTTPServer):
    daemon_threads = True

    def handle_error(self, request, client_address):
        pass   # cancelled requests reset the socket on purpose


def main():
    srv = Server(("127.0.0.1", 0), Handler)
    srv.daemon_threads = True
    with open(sys.argv[1], "w") as f:
        f.write("http://127.0.0.1:%d\n" % srv.server_address[1])
    srv.serve_forever()


if __name__ == "__main__":
    main()
