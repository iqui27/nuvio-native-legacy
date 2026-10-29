#include <assert.h>
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
    return failures ? 1 : 0;
  }
}
