#include <assert.h>
static int cancelLater(void *context) {
  return NSDate.timeIntervalSinceReferenceDate - *(NSTimeInterval *)context > .15;
}
#ifndef NV_HTTP_CANCEL_TEST
#define NV_HTTP_CANCEL_TEST 0
#endif
int main(int argc, char **argv) {
  @autoreleasepool {
    (void)argc;
    int failures = 0;
    for (NSString *path in @[@"/0", @"/8192", @"/8193", @"/stream"]) {
      NSString *url = [NSString stringWithFormat:@"http://127.0.0.1:%s%@", argv[1], path];
      int status = 0, retry = 0;
      long n = 0;
      char error[200];
      NSTimeInterval start = NSDate.timeIntervalSinceReferenceDate;
      char *body = nv_host_http_limit("GET", url.UTF8String, NULL, NULL, 5000,
                                     8192, &status, &retry, &n, error, sizeof error);
      BOOL over = [path isEqualToString:@"/8193"] || [path isEqualToString:@"/stream"];
      BOOL ok = status == 200 && retry == 7;
      if (over) ok = ok && !body && n == -1;
      else ok = ok && body && n == [path substringFromIndex:1].intValue && body[n] == 0;
      if ([path isEqualToString:@"/stream"])
        ok = ok && NSDate.timeIntervalSinceReferenceDate - start < 1.0;
      printf("%s HTTP %s (bytes=%ld)\n", ok ? "PASS" : "FAIL", path.UTF8String, n);
      failures += !ok;
      free(body);
    }
    for (NSString *path in @[@"/same", @"/redirect/302/echo", @"/redirect/302/redirect/302/echo",
                              @"/redirect/302/echo?post", @"/redirect/307/echo", @"/redirect/308/echo"]) {
      BOOL post = [path containsString:@"post"] || [path containsString:@"307"] || [path containsString:@"308"];
      NSString *url = [NSString stringWithFormat:@"http://127.0.0.1:%s%@", argv[1], [path stringByReplacingOccurrencesOfString:@"?post" withString:@""]];
      int status; long n; char error[200];
      char *body = nv_host_http_limit(post ? "POST" : "GET", url.UTF8String,
          "Authorization: Bearer secret\nCookie: session=secret\nX-Api-Key: secret", post ? "secret-body" : NULL,
          2000, 8192, &status, NULL, &n, error, sizeof error);
      BOOL reject = [path containsString:@"307"] || [path containsString:@"308"];
      const char *expected = [path isEqualToString:@"/same"] ? "GET|Bearer secret|session=secret|secret|" : "GET||||";
      BOOL ok = reject ? body == NULL : body && !strcmp(body, expected);
      printf("%s redirect %s\n", ok ? "PASS" : "FAIL", path.UTF8String);
      failures += !ok; free(body);
    }
    for (int cancel = 0; cancel < 2; cancel++) {
      NSString *url = [NSString stringWithFormat:@"http://127.0.0.1:%s/%s", argv[1], cancel ? "idle" : "trickle"];
      NSTimeInterval start = NSDate.timeIntervalSinceReferenceDate;
      int status; long n; char error[200];
#if NV_HTTP_CANCEL_TEST
      char *body = nv_host_http_limit_cancel("GET", url.UTF8String, NULL, NULL,
          cancel ? 2000 : 200, 8192, cancel ? cancelLater : NULL, &start,
          &status, NULL, &n, error, sizeof error);
#else
      char *body = nv_host_http_limit("GET", url.UTF8String, NULL, NULL,
          cancel ? 2000 : 200, 8192, &status, NULL, &n, error, sizeof error);
#endif
      double elapsed = NSDate.timeIntervalSinceReferenceDate - start;
      BOOL ok = !body && elapsed < .8;
      printf("%s HTTP %s (%.3fs)\n", ok ? "PASS" : "FAIL", cancel ? "in-flight cancellation" : "total deadline", elapsed);
      failures += !ok; free(body);
    }
    return failures ? 1 : 0;
  }
}
