#include <TargetConditionals.h>
#if defined(__APPLE__) && TARGET_OS_IPHONE
#include "http.h"
#include "progress.h"

#import <Foundation/Foundation.h>

#include <fstream>
#include <string_view>

// HTTPS through NSURLSession on iOS: the system TLS stack and certificate store, no libcurl. Same contract
// as posix_http.cpp (curl) and android_http.cpp (HttpURLConnection): HTTPS only, at most three redirects, all
// of them HTTPS, a size-limited body for ReadResponse, and a progress callback that can cancel a download.
// The calling thread blocks; the session delivers on its own queue, so this is safe from any thread.

@interface LOTransfer : NSObject <NSURLSessionDataDelegate>
{
@public
    std::string *body;                          // ReadResponse target (nullptr for downloads)
    size_t limit;
    bool exceeded;
    std::ofstream *output;                      // DownloadFile target (nullptr for reads)
    const updater::DownloadProgress *progress;
    uint64_t expectedSize;
    uint64_t total;
    bool cancelled;
    bool writeFailed;
    int redirects;
    bool insecureRedirect;
    long status;
    NSString *failure;
    dispatch_semaphore_t done;
}
@end

@implementation LOTransfer
- (instancetype)init
{
    if ((self = [super init]))
    {
        body = nullptr; limit = 0; exceeded = false; output = nullptr; progress = nullptr; expectedSize = 0;
        total = 0; cancelled = false; writeFailed = false; redirects = 0; insecureRedirect = false; status = 0;
        failure = nil;
        done = dispatch_semaphore_create(0);
    }
    return self;
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task
    didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition))handler
{
    if ([response isKindOfClass:[NSHTTPURLResponse class]])
        status = (long)((NSHTTPURLResponse *)response).statusCode;
    handler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)task didReceiveData:(NSData *)data
{
    const size_t bytes = (size_t)data.length;
    if (body)
    {
        if (bytes > limit - body->size()) { exceeded = true; [task cancel]; return; }
        body->append((const char *)data.bytes, bytes);
        return;
    }
    if (output)
    {
        output->write((const char *)data.bytes, (std::streamsize)bytes);
        if (!*output) { writeFailed = true; [task cancel]; return; }
        total += bytes;
        if (progress && !(*progress)(total, expectedSize)) { cancelled = true; [task cancel]; }
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
    willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request
    completionHandler:(void (^)(NSURLRequest *))handler
{
    ++redirects;
    if (redirects > 3 || ![request.URL.scheme isEqualToString:@"https"])
    {
        insecureRedirect = true;
        handler(nil);   // stop; the 3xx response completes the task and is reported as an error below
        return;
    }
    handler(request);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error
{
    if (error && !cancelled && !exceeded && !writeFailed)
        failure = error.localizedDescription ?: @"request failed";
    dispatch_semaphore_signal(done);
}
@end

namespace updater
{
namespace
{
constexpr std::string_view UserAgent = "LostOdysseyRecomp-Updater/1.0";

bool IsHttpsUrl(std::string_view url)
{
    constexpr std::string_view prefix = "https://";
    return url.size() >= prefix.size() && url.substr(0, prefix.size()) == prefix;
}

// Runs the transfer to completion. requestTimeout is the idle limit between data; resourceTimeout bounds the whole transfer.
void Run(LOTransfer *transfer, std::string_view url, NSTimeInterval requestTimeout, NSTimeInterval resourceTimeout)
{
    @autoreleasepool
    {
        NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        configuration.timeoutIntervalForRequest = requestTimeout;
        configuration.timeoutIntervalForResource = resourceTimeout;
        configuration.HTTPAdditionalHeaders = @{ @"User-Agent": [NSString stringWithUTF8String:UserAgent.data()] };
        NSOperationQueue *queue = [[NSOperationQueue alloc] init];
        queue.maxConcurrentOperationCount = 1;
        NSURLSession *session = [NSURLSession sessionWithConfiguration:configuration delegate:transfer delegateQueue:queue];
        NSURL *target = [NSURL URLWithString:[[NSString alloc] initWithBytes:url.data() length:url.size() encoding:NSUTF8StringEncoding]];
        NSURLSessionDataTask *task = target ? [session dataTaskWithURL:target] : nil;
        if (!task)
        {
            transfer->failure = @"invalid URL";
            [session invalidateAndCancel];
            return;
        }
        [task resume];
        dispatch_semaphore_wait(transfer->done, DISPATCH_TIME_FOREVER);
        [session finishTasksAndInvalidate];
    }
}

std::string Describe(NSString *text) { return text ? std::string(text.UTF8String) : std::string(); }
}

bool ReadResponse(std::string_view url, size_t limit, std::string &body, std::string &error)
{
    body.clear();
    if (!IsHttpsUrl(url)) { error = "update URL does not use HTTPS"; return false; }
    LOTransfer *transfer = [[LOTransfer alloc] init];
    transfer->body = &body;
    transfer->limit = limit;
    Run(transfer, url, 8.0, 8.0);
    if (transfer->exceeded) { error = "update response exceeded its size limit"; return false; }
    if (transfer->insecureRedirect) { error = "update redirect resolved to non-HTTPS URL"; return false; }
    if (transfer->failure) { error = "request failed: " + Describe(transfer->failure); return false; }
    if (transfer->status != 200) { error = "update server returned HTTP " + std::to_string(transfer->status); return false; }
    return true;
}

bool DownloadFile(std::string_view url, const std::filesystem::path &destination, uint64_t expectedSize,
                  const DownloadProgress &progress, std::string &error, bool &cancelled)
{
    cancelled = false;
    if (!IsHttpsUrl(url)) { error = "update URL does not use HTTPS"; return false; }
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) { error = "could not create update download"; return false; }
    LOTransfer *transfer = [[LOTransfer alloc] init];
    transfer->output = &output;
    transfer->progress = &progress;
    transfer->expectedSize = expectedSize;
    // No overall limit for large files, but a stalled transfer (30 s without data) fails.
    Run(transfer, url, 30.0, 7.0 * 24.0 * 3600.0);
    if (transfer->cancelled) { cancelled = true; error = "update cancelled by user"; return false; }
    if (transfer->writeFailed) { error = "could not write update download"; return false; }
    if (transfer->insecureRedirect) { error = "update redirect resolved to non-HTTPS URL"; return false; }
    if (transfer->failure) { error = "request failed: " + Describe(transfer->failure); return false; }
    if (transfer->status != 200) { error = "update server returned HTTP " + std::to_string(transfer->status); return false; }
    output.flush();
    if (!output) { error = "could not write update download"; return false; }
    return true;
}

bool Download(std::string_view url, const std::filesystem::path &destination, uint64_t expectedSize,
              ProgressWindow &progress, std::string &error, bool &cancelled)
{
    return DownloadFile(url, destination, expectedSize, [&progress](uint64_t completed, uint64_t total) {
        if (completed) progress.SetDownloadProgress(completed, total);
        return !progress.Cancelled();
    }, error, cancelled);
}
}
#endif
