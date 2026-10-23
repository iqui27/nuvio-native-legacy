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
        if (a < 0) return s;
        int b = s.indexOf('/', a + 2);
        int fim = -1;
        int[] cortes = {s.indexOf('?'), s.indexOf('#')};
        for (int c : cortes) if (c >= 0 && (fim < 0 || c < fim)) fim = c;
        if (b < 0) return "";
        return fim < 0 || fim < b ? s.substring(b) : s.substring(b, fim);
    }

    @Override public boolean equals(Object o) { return o instanceof Uri && s.equals(o.toString()); }
    @Override public int hashCode() { return s.hashCode(); }
}
