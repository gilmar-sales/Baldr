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

    void Handle(const HttpRequest&    request,
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
