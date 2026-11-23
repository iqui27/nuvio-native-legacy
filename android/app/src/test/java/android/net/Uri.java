package android.net;

// JVM unit tests only: the mockable android.jar returns null from Uri.parse()
// (and android.net.Uri() is package-private, so no subclass fakes it either),
// which makes every DataSpec fail with "The uri must be set". This minimal
// concrete Uri lets the tests call DataSource.open() on the JVM. The device
// build never sees this file: it lives only on the unit-test classpath.
public class Uri {
    private final String s;

    private Uri(String s) { this.s = s; }

    public static Uri parse(String s) { return s == null ? null : new Uri(s); }

    @Override public String toString() { return s; }

    private String authority() {
        int a = s.indexOf("//");
        if (a < 0) return null;
        int fim = -1;
        int[] cortes = {s.indexOf('/', a + 2), s.indexOf('?', a + 2), s.indexOf('#', a + 2)};
        for (int c : cortes) if (c >= 0 && (fim < 0 || c < fim)) fim = c;
        return fim < 0 ? s.substring(a + 2) : s.substring(a + 2, fim);
    }

    public String getScheme() {
        int i = s.indexOf(':');
        return i < 0 ? null : s.substring(0, i);
    }

    public String getHost() {
        String a = authority();
        if (a == null) return null;
        int arroba = a.lastIndexOf('@');
        String h = arroba < 0 ? a : a.substring(arroba + 1);
        int dois = h.indexOf(':');
        return dois < 0 ? h : h.substring(0, dois);
    }

    public int getPort() {
        String a = authority();
        if (a == null) return -1;
        int dois = a.lastIndexOf(':');
        if (dois < 0) return -1;
        try { return Integer.parseInt(a.substring(dois + 1)); }
        catch (NumberFormatException e) { return -1; }
    }

    public String getAuthority() { return authority(); }

    public String getPath() {
        int a = s.indexOf("//");
        if (a < 0) {
            int fim = -1;
            int[] cortes = {s.indexOf('?'), s.indexOf('#')};
            for (int c : cortes) if (c >= 0 && (fim < 0 || c < fim)) fim = c;
            return fim < 0 ? s : s.substring(0, fim);
        }
        int b = s.indexOf('/', a + 2);
        int fim = -1;
        int[] cortes = {s.indexOf('?'), s.indexOf('#')};
        for (int c : cortes) if (c >= 0 && (fim < 0 || c < fim)) fim = c;
        if (b < 0) return "";
        return fim < 0 || fim < b ? s.substring(b) : s.substring(b, fim);
    }

    public String getLastPathSegment() {
        String p = getPath();
        if (p == null) return null;
        return p.substring(p.lastIndexOf('/') + 1);
    }

    // Decoded query like android.net.Uri.getQuery(): between '?' and '#',
    // percent-escapes resolved. Values inside it may carry a nested encoded
    // query (file=canal.mpd%3Ftoken%3Dabc -> "file=canal.mpd?token=abc").
    public String getQuery() {
        int q = s.indexOf('?');
        if (q < 0) return null;
        int f = s.indexOf('#');
        String raw = f < 0 || f < q ? s.substring(q + 1) : s.substring(q + 1, f);
        return decode(raw);
    }

    private static String decode(String v) {
        int p = v.indexOf('%');
        if (p < 0) return v;
        StringBuilder out = new StringBuilder(v.length());
        java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
        int i = 0;
        while (i < v.length()) {
            char c = v.charAt(i);
            if (c == '%') {
                bytes.reset();
                int start = i;
                while (i + 2 < v.length() && v.charAt(i) == '%') {
                    int hi = Character.digit(v.charAt(i + 1), 16);
                    int lo = Character.digit(v.charAt(i + 2), 16);
                    if (hi < 0 || lo < 0) break;
                    bytes.write((hi << 4) | lo);
                    i += 3;
                }
                if (i == start) {
                    // Malformed escape: the real UriCodec appends U+FFFD once
                    // and consumes '%' plus only the first offending hex char
                    // ("%ZZ" -> "�Z", "%" at end -> "�"). Never hang on '%'.
                    out.append('�');
                    i++;
                    if (i < v.length() && v.charAt(i) != '%') i++;
                } else {
                    out.append(new String(bytes.toByteArray(), java.nio.charset.StandardCharsets.UTF_8));
                }
            } else {
                out.append(c);
                i++;
            }
        }
        return out.toString();
    }

    @Override public boolean equals(Object o) { return o instanceof Uri && s.equals(o.toString()); }
    @Override public int hashCode() { return s.hashCode(); }
}
