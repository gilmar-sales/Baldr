#include <Baldr/Detail/Namespace.hpp>
#include "OpenApiSpecService.hpp"

namespace BALDR_NAMESPACE {

const std::string& OpenApiSpecService::Cached(const skr::Arc<Router>& router)
{
    if (!mRendered)
        Regenerate(router);
    return mCache;
}

void OpenApiSpecService::Regenerate(const skr::Arc<Router>& router)
{
    auto entries = router->Snapshot();

    const SchemaRegistry* reg    = nullptr;
    const auto&           shared = router->SchemaRegistrySlot();
    if (shared && !shared->Schemas().empty())
    {
        reg = shared.get();
    }
    else
    {
        reg = &mRegistry;
    }

    SpecBuilder builder(mOptions);
    builder.SetRegistry(*reg);
    mCache    = builder.Render(entries);
    mRendered = true;
}

} // namespace BALDR_NAMESPACE