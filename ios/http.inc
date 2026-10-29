// NSURLSession delivers chunks on a serial delegate queue. The returned C
// buffer is the receive buffer; no second full response allocation is needed.
@interface NuvioHTTPReceiver : NSObject <NSURLSessionDataDelegate> {
@public
  char *body;
  size_t length, capacity, limit;
  BOOL exceeded, isolated;
  unsigned redirects;
  NSHTTPURLResponse *response;
  NSError *failure;
  dispatch_semaphore_t done;
}
@end
@implementation NuvioHTTPReceiver
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
 willPerformHTTPRedirection:(NSHTTPURLResponse *)redirect newRequest:(NSURLRequest *)request
 completionHandler:(void (^)(NSURLRequest *))complete {
  (void)session;
  NSURL *from = redirect.URL, *to = request.URL;
  NSNumber *fromPort = from.port ?: @([from.scheme.lowercaseString isEqualToString:@"https"] ? 443 : 80);
  NSNumber *toPort = to.port ?: @([to.scheme.lowercaseString isEqualToString:@"https"] ? 443 : 80);
  BOOL cross = ![from.scheme.lowercaseString isEqual:to.scheme.lowercaseString] ||
               ![from.host.lowercaseString isEqual:to.host.lowercaseString] || ![fromPort isEqual:toPort];
  BOOL safe = [request.HTTPMethod isEqualToString:@"GET"] || [request.HTTPMethod isEqualToString:@"HEAD"];
  if (++redirects > 5 || (![to.scheme.lowercaseString isEqualToString:@"http"] &&
                         ![to.scheme.lowercaseString isEqualToString:@"https"]) ||
      ([from.scheme.lowercaseString isEqualToString:@"https"] && ![to.scheme.lowercaseString isEqualToString:@"https"]) ||
      (cross && (!safe || request.HTTPBody || request.HTTPBodyStream))) {
    failure = [NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorHTTPTooManyRedirects userInfo:nil];
    complete(nil); [task cancel]; return;
  }
  isolated |= cross;
  // A fresh request prevents Foundation from restoring stripped headers.
  NSMutableURLRequest *next = isolated ? [NSMutableURLRequest requestWithURL:to] : [request mutableCopy];
  next.HTTPMethod = request.HTTPMethod;
  next.timeoutInterval = request.timeoutInterval;
  next.cachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
  if (isolated) next.HTTPShouldHandleCookies = NO;
  else next.allHTTPHeaderFields = task.originalRequest.allHTTPHeaderFields;
  complete(next);
}
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task
 didReceiveResponse:(NSURLResponse *)r completionHandler:(void (^)(NSURLSessionResponseDisposition))complete {
  (void)session; (void)task;
  if ([r isKindOfClass:NSHTTPURLResponse.class]) response = (NSHTTPURLResponse *)r;
  complete(NSURLSessionResponseAllow);
}
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task didReceiveData:(NSData *)data {
  (void)session;
  if (exceeded || failure) return;
  if (data.length > limit - length) {
    exceeded = YES;
    [task cancel];
    return;
  }
  size_t needed = length + data.length + 1;
  if (needed > capacity) {
    size_t next = MAX(needed, MIN(limit + 1, capacity ? capacity * 2 : 4096));
    char *grown = realloc(body, next);
    if (!grown) {
      failure = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
      [task cancel];
      return;
    }
    body = grown; capacity = next;
  }
  memcpy(body + length, data.bytes, data.length);
  length += data.length;
}
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error {
  (void)session; (void)task;
  if (!failure) failure = error;
  dispatch_semaphore_signal(done);
}
@end

char *nv_host_http_limit_cancel(const char *verbo, const char *url, const char *cabs,
                         const char *corpo, int prazoMs, size_t maxBytes,
                         int (*parar)(void *), void *context,
                         int *status, int *retryAfter, long *n, char *erro, size_t nErro) {
  if (status) *status = 0;
  if (retryAfter) *retryAfter = 0;
  if (n) *n = 0;
  if (erro && nErro) erro[0] = 0;
  NSURL *u = [NSURL URLWithString:[NSString stringWithUTF8String:url ? url : ""]];
  if (!u) { if (erro) snprintf(erro, nErro, "url invalida"); return NULL; }
  NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:u];
  req.HTTPMethod = [NSString stringWithUTF8String:verbo && *verbo ? verbo : "GET"];
  req.timeoutInterval = (prazoMs > 0 ? prazoMs : 20000) / 1000.0;
  req.cachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
  if (cabs) {
    for (NSString *linha in [[NSString stringWithUTF8String:cabs] componentsSeparatedByString:@"\n"]) {
      NSRange dp = [linha rangeOfString:@":"];
      if (dp.location == NSNotFound) continue;
      NSString *k = [[linha substringToIndex:dp.location] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
      NSString *v = [[linha substringFromIndex:dp.location + 1] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
      if (k.length) [req setValue:v forHTTPHeaderField:k];
    }
  }
  if (corpo) req.HTTPBody = [NSData dataWithBytes:corpo length:strlen(corpo)];
  NuvioHTTPReceiver *receiver = [NuvioHTTPReceiver new];
  receiver->limit = MIN(maxBytes, (size_t)LONG_MAX - 1);
  receiver->done = dispatch_semaphore_create(0);
  NSURLSessionConfiguration *config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
  config.HTTPMaximumConnectionsPerHost = 6;
  config.waitsForConnectivity = NO;
  config.timeoutIntervalForResource = req.timeoutInterval;
  NSURLSession *session = [NSURLSession sessionWithConfiguration:config delegate:receiver delegateQueue:nil];
  NSURLSessionDataTask *task = [session dataTaskWithRequest:req];
  dispatch_time_t deadline = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(prazoMs > 0 ? prazoMs : 20000) * NSEC_PER_MSEC);
  BOOL stopped = NO;
  [task resume];
  while (dispatch_semaphore_wait(receiver->done, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_MSEC))) {
    if ((parar && parar(context)) || dispatch_time(DISPATCH_TIME_NOW, 0) >= deadline) {
      stopped = YES;
      [task cancel];
      // Completion serializes delegate writes before the buffer is freed.
      dispatch_semaphore_wait(receiver->done, DISPATCH_TIME_FOREVER);
      break;
    }
  }
  [session finishTasksAndInvalidate];
  if (status) *status = (int)receiver->response.statusCode;
  NSString *ra = receiver->response.allHeaderFields[@"Retry-After"];
  if (retryAfter && ra) *retryAfter = ra.intValue;
  if (stopped || receiver->exceeded || receiver->failure || !receiver->response) {
    if (receiver->exceeded && n) *n = -1;
    if (erro) snprintf(erro, nErro, "%s", receiver->exceeded ? "body limit exceeded" :
                       receiver->failure ? receiver->failure.localizedDescription.UTF8String : "sem resposta");
    free(receiver->body);
    return NULL;
  }
  if (!receiver->body) receiver->body = malloc(1);
  if (!receiver->body) { if (erro) snprintf(erro, nErro, "sem memoria"); return NULL; }
  receiver->body[receiver->length] = 0;
  if (n) *n = (long)receiver->length;
  return receiver->body;
}

char *nv_host_http_limit(const char *verbo, const char *url, const char *cabs,
                         const char *corpo, int prazoMs, size_t maxBytes,
                         int *status, int *retryAfter, long *n, char *erro, size_t nErro) {
  return nv_host_http_limit_cancel(verbo, url, cabs, corpo, prazoMs, maxBytes, NULL, NULL,
                                   status, retryAfter, n, erro, nErro);
}

char *nv_host_http(const char *verbo, const char *url, const char *cabs,
                   const char *corpo, int prazoMs, int *status, int *retryAfter, long *n,
                   char *erro, size_t nErro) {
  return nv_host_http_limit(verbo, url, cabs, corpo, prazoMs, (size_t)LONG_MAX - 1,
                            status, retryAfter, n, erro, nErro);
}
