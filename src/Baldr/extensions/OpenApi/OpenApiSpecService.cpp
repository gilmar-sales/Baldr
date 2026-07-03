#include "OpenApiSpecService.hpp"

namespace Baldr::OpenApi
{
    const std::string& OpenApiSpecService::Cached(const skr::Arc<Router>& router)
    {
        if (!mRendered)
            Regenerate(router);
        return mCache;
    }

    void OpenApiSpecService::Regenerate(const skr::Arc<Router>& router)
    {
        auto entries = router->Snapshot();
        SpecBuilder builder(mOptions);
        builder.SetRegistry(mRegistry);
        mCache    = builder.Render(entries);
        mRendered = true;
    }
}