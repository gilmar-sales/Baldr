#include "Baldr/HttpConnection.hpp"
#include "Baldr/HttpRequest.hpp"
#include "Baldr/HttpResponse.hpp"
#include "Baldr/IMiddleware.hpp"
#include "Baldr/MiddlewareProvider.hpp"
#include "Baldr/Router.hpp"

#include <atomic>
#include <vector>

namespace
{
class FakeMiddleware : public IMiddleware
{
  public:
    explicit FakeMiddleware(std::function<void(NextMiddleware)> body) :
        mBody(std::move(body))
    {
    }

    void Handle(HttpRequest&          request,
                HttpResponse&         response,
                const NextMiddleware& next) override
    {
        mBody(next);
    }

  private:
    std::function<void(NextMiddleware)> mBody;
};
}

class HttpConnectionMiddlewareChainSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mRequest.method   = HttpMethod::Get;
        mRequest.path     = "/api/test";
        mRequest.version  = "HTTP/1.1";
        mRequest.clientIp = "127.0.0.1";
        mResponse         = HttpResponse(mRequest);
    }

    static MiddlewareFactory makeFactory(std::function<void(NextMiddleware)> body)
    {
        return [body = std::move(body)](
                   const skr::Arc<skr::ServiceProvider>&) {
            return skr::MakeArc<FakeMiddleware>(std::move(body));
        };
    }

    HttpRequest  mRequest;
    HttpResponse mResponse;
    skr::Arc<skr::ServiceProvider> mEmptyProvider =
        skr::MakeArc<skr::ServiceCollection>()->CreateServiceProvider();
};

TEST_F(HttpConnectionMiddlewareChainSpec,
       ShouldRunFinalHandlerWhenNoMiddlewaresRegistered)
{
    MiddlewareFactoryList factories;

    std::atomic<bool> handlerRan { false };
    RouteHandler       finalHandler =
        [&](HttpRequest&, HttpResponse&,
            const skr::Arc<skr::ServiceProvider>&) { handlerRan = true; };

    HttpConnection::runMiddlewareChain(factories, mEmptyProvider, mRequest,
                                       mResponse, finalHandler);

    ASSERT_TRUE(handlerRan.load());
}

TEST_F(HttpConnectionMiddlewareChainSpec,
       ShouldRunAllMiddlewaresInOrderBeforeFinalHandler)
{
    MiddlewareFactoryList factories;
    std::vector<int>      order;
    std::atomic<int>      counter { 0 };

    factories.push_back(makeFactory([&](NextMiddleware next) {
        int id = ++counter;
        order.push_back(id);
        next();
    }));
    factories.push_back(makeFactory([&](NextMiddleware next) {
        int id = ++counter;
        order.push_back(id);
        next();
    }));

    std::atomic<bool> handlerRan { false };
    RouteHandler       finalHandler =
        [&](HttpRequest&, HttpResponse&,
            const skr::Arc<skr::ServiceProvider>&) {
            int id = ++counter;
            order.push_back(id);
            handlerRan = true;
        };

    HttpConnection::runMiddlewareChain(factories, mEmptyProvider, mRequest,
                                       mResponse, finalHandler);

    ASSERT_TRUE(handlerRan.load());
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST_F(HttpConnectionMiddlewareChainSpec,
       ShouldShortCircuitWhenMiddlewareSkipsNext)
{
    MiddlewareFactoryList factories;
    std::atomic<int>      counter { 0 };

    factories.push_back(makeFactory([&](NextMiddleware) {
        ++counter;
    }));
    factories.push_back(makeFactory([&](NextMiddleware) {
        ++counter;
    }));

    std::atomic<bool> handlerRan { false };
    RouteHandler       finalHandler =
        [&](HttpRequest&, HttpResponse&,
            const skr::Arc<skr::ServiceProvider>&) { handlerRan = true; };

    HttpConnection::runMiddlewareChain(factories, mEmptyProvider, mRequest,
                                       mResponse, finalHandler);

    EXPECT_EQ(counter.load(), 1);
    EXPECT_FALSE(handlerRan.load());
}

// ============================================================================
// Request/response observation through middleware
// ============================================================================
//
// These tests observe how data flows through the middleware chain by having
// the FakeMiddleware read the request and mutate the response. They are the
// closest unit-level analogue to the LoggingMiddleware behaviour used in
// production (e.g. Devices example): a middleware that captures
// `response.statusCode` after `next()` returns and compares it with what the
// final handler actually wrote.
//
// The chain uses onion semantics: when a middleware calls next(), the rest
// of the chain (subsequent middlewares and the final handler) runs
// synchronously before next() returns. That is what makes the Devices
// example log `Response - '200'` instead of the previous broken behaviour
// where the log always reported `Response - '404'` (the HttpResponse
// default), regardless of what the route handler did.

TEST_F(HttpConnectionMiddlewareChainSpec,
       MiddlewareObservesRequestFieldsBeforeAndAfterNext)
{
    MiddlewareFactoryList factories;

    HttpRequest  seenRequest;
    HttpResponse seenResponse;
    std::atomic<bool> nextReturned { false };

    factories.push_back(makeFactory([&](NextMiddleware next) {
        // Pre-next snapshot
        EXPECT_EQ(mRequest.method, HttpMethod::Get);
        EXPECT_EQ(mRequest.path, "/api/test");
        EXPECT_EQ(mRequest.version, "HTTP/1.1");
        EXPECT_EQ(mRequest.clientIp, "127.0.0.1");

        next();
        nextReturned = true;

        // Post-next: request must be unchanged (HttpRequest is taken by const&)
        EXPECT_EQ(mRequest.method, HttpMethod::Get);
        EXPECT_EQ(mRequest.path, "/api/test");
    }));

    RouteHandler finalHandler =
        [&](HttpRequest& request, HttpResponse& response,
            const skr::Arc<skr::ServiceProvider>&) {
            seenRequest  = request;
            seenResponse = response;
        };

    HttpConnection::runMiddlewareChain(factories, mEmptyProvider, mRequest,
                                       mResponse, finalHandler);

    ASSERT_TRUE(nextReturned.load());
    EXPECT_EQ(seenRequest.path, "/api/test");
}

TEST_F(HttpConnectionMiddlewareChainSpec,
       MiddlewareObservesHandlerMutationAfterNext)
{
    // Pin the standard "onion" middleware semantics: when a middleware
    // calls next(), the rest of the chain runs synchronously before
    // next() returns. By the time next() returns, the final handler has
    // already mutated `response`. This is the contract that
    // LoggingMiddleware relies on to log the real response status code
    // (not the HttpResponse default of NotFound) for the request log
    // line — see the Devices example.
    MiddlewareFactoryList factories;

    std::atomic<int>  observedStatusBeforeNext { -1 };
    std::atomic<int>  observedStatusAfterNext  { -1 };
    std::string       observedBodyAfterNext;
    std::atomic<bool> nextReturned { false };

    factories.push_back(makeFactory([&](NextMiddleware next) {
        observedStatusBeforeNext =
            static_cast<int>(mResponse.statusCode);
        next();
        nextReturned = true;
        observedStatusAfterNext =
            static_cast<int>(mResponse.statusCode);
        observedBodyAfterNext = mResponse.body;
    }));

    RouteHandler finalHandler =
        [&](HttpRequest&, HttpResponse& response,
            const skr::Arc<skr::ServiceProvider>&) {
            response.statusCode = StatusCode::OK;
            response.body       = "hello";
            response.headers["Content-Type"] = "plain/text";
        };

    HttpConnection::runMiddlewareChain(factories, mEmptyProvider, mRequest,
                                       mResponse, finalHandler);

    // Before next(): response carries its default (NotFound).
    EXPECT_EQ(observedStatusBeforeNext.load(),
              static_cast<int>(StatusCode::NotFound));
    // After next(): the final handler has run, so the middleware sees
    // the real status code and body.
    EXPECT_TRUE(nextReturned.load());
    EXPECT_EQ(observedStatusAfterNext.load(),
              static_cast<int>(StatusCode::OK));
    EXPECT_EQ(observedBodyAfterNext, "hello");
    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::OK));
    EXPECT_EQ(mResponse.body, "hello");
}

TEST_F(HttpConnectionMiddlewareChainSpec,
       MiddlewareObservesHandlerMutationEvenWithNestedMiddlewares)
{
    // Same onion contract, but with two middlewares wrapping the final
    // handler. After the outer middleware's next() returns, both the
    // inner middleware and the final handler have run, so the outer
    // middleware observes the post-handler response.
    MiddlewareFactoryList factories;

    std::atomic<int> outerAfterNext { -1 };
    std::string      outerBodyAfterNext;
    std::atomic<int> innerAfterNext { -1 };

    factories.push_back(makeFactory([&](NextMiddleware next) {
        next();
        outerAfterNext     = static_cast<int>(mResponse.statusCode);
        outerBodyAfterNext = mResponse.body;
    }));
    factories.push_back(makeFactory([&](NextMiddleware next) {
        next();
        innerAfterNext = static_cast<int>(mResponse.statusCode);
    }));

    RouteHandler finalHandler =
        [&](HttpRequest&, HttpResponse& response,
            const skr::Arc<skr::ServiceProvider>&) {
            response.statusCode = StatusCode::OK;
            response.body       = "world";
        };

    HttpConnection::runMiddlewareChain(factories, mEmptyProvider, mRequest,
                                       mResponse, finalHandler);

    // Inner middleware: next() awaits the final handler, so it sees OK.
    EXPECT_EQ(innerAfterNext.load(), static_cast<int>(StatusCode::OK));
    // Outer middleware: next() awaits everything below (inner + handler),
    // so it also sees OK and the handler's body.
    EXPECT_EQ(outerAfterNext.load(), static_cast<int>(StatusCode::OK));
    EXPECT_EQ(outerBodyAfterNext, "world");
}

TEST_F(HttpConnectionMiddlewareChainSpec,
       MiddlewareCanMutateResponseBeforeFinalHandler)
{
    MiddlewareFactoryList factories;

    factories.push_back(makeFactory([&](NextMiddleware next) {
        mResponse.statusCode          = StatusCode::Accepted;
        mResponse.headers["X-Pre"]    = "middleware";
        next();
    }));

    std::atomic<int> seenStatus { -1 };
    std::atomic<bool> seenHeader { false };
    RouteHandler finalHandler =
        [&](HttpRequest&, HttpResponse& response,
            const skr::Arc<skr::ServiceProvider>&) {
            seenStatus = static_cast<int>(response.statusCode);
            seenHeader =
                response.headers.find("X-Pre") != response.headers.end();
        };

    HttpConnection::runMiddlewareChain(factories, mEmptyProvider, mRequest,
                                       mResponse, finalHandler);

    EXPECT_EQ(seenStatus.load(), static_cast<int>(StatusCode::Accepted));
    EXPECT_TRUE(seenHeader.load());
}

TEST_F(HttpConnectionMiddlewareChainSpec,
       ResponseDefaultsToNotFoundWhenHandlerDoesNotMutate)
{
    // The default HttpResponse(request) constructor sets statusCode to
    // NotFound. If a handler is registered but does not assign a status
    // code, the connection code must still send *something*; the contract
    // is that callers must always set statusCode in their handler.
    MiddlewareFactoryList factories;

    std::atomic<int> observedStatusAfterNext { -1 };

    factories.push_back(makeFactory([&](NextMiddleware next) {
        next();
        observedStatusAfterNext =
            static_cast<int>(mResponse.statusCode);
    }));

    RouteHandler finalHandler =
        [&](HttpRequest&, HttpResponse&,
            const skr::Arc<skr::ServiceProvider>&) {
            // Intentionally do nothing.
        };

    HttpConnection::runMiddlewareChain(factories, mEmptyProvider, mRequest,
                                       mResponse, finalHandler);

    EXPECT_EQ(observedStatusAfterNext.load(),
              static_cast<int>(StatusCode::NotFound));
}

TEST_F(HttpConnectionMiddlewareChainSpec,
       MiddlewareCanShortCircuitAndSetResponse)
{
    MiddlewareFactoryList factories;

    factories.push_back(makeFactory([&](NextMiddleware) {
        mResponse.statusCode = StatusCode::Unauthorized;
        mResponse.body       = "denied";
    }));
    factories.push_back(makeFactory([&](NextMiddleware next) {
        next(); // should never run
    }));

    std::atomic<bool> handlerRan { false };
    RouteHandler finalHandler =
        [&](HttpRequest&, HttpResponse&,
            const skr::Arc<skr::ServiceProvider>&) { handlerRan = true; };

    HttpConnection::runMiddlewareChain(factories, mEmptyProvider, mRequest,
                                       mResponse, finalHandler);

    EXPECT_FALSE(handlerRan.load());
    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::Unauthorized));
    EXPECT_EQ(mResponse.body, "denied");
}

TEST_F(HttpConnectionMiddlewareChainSpec,
       FinalHandlerReceivesScopedServiceProvider)
{
    MiddlewareFactoryList factories;

    std::atomic<bool> providerNonNull { false };
    RouteHandler finalHandler =
        [&](HttpRequest&, HttpResponse&,
            const skr::Arc<skr::ServiceProvider>& sp) {
            providerNonNull = sp != nullptr;
        };

    HttpConnection::runMiddlewareChain(factories, mEmptyProvider, mRequest,
                                       mResponse, finalHandler);

    EXPECT_TRUE(providerNonNull.load());
}

TEST_F(HttpConnectionMiddlewareChainSpec,
       HeadersAndBodySurviveTheChain)
{
    MiddlewareFactoryList factories;

    factories.push_back(makeFactory([&](NextMiddleware next) {
        mResponse.headers["X-Mw"] = "1";
        next();
    }));

    RouteHandler finalHandler =
        [&](HttpRequest&, HttpResponse& response,
            const skr::Arc<skr::ServiceProvider>&) {
            response.headers["Content-Type"] = "application/json";
            response.body                    = R"({"ok":true})";
            response.statusCode              = StatusCode::OK;
        };

    HttpConnection::runMiddlewareChain(factories, mEmptyProvider, mRequest,
                                       mResponse, finalHandler);

    EXPECT_EQ(mResponse.headers.at("X-Mw"), "1");
    EXPECT_EQ(mResponse.headers.at("Content-Type"), "application/json");
    EXPECT_EQ(mResponse.body, R"({"ok":true})");
    EXPECT_EQ(static_cast<int>(mResponse.statusCode),
              static_cast<int>(StatusCode::OK));
}
