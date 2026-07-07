/**
 * @file BaldrExtension.hpp
 * @brief Top-level Skirnir extension that wires Baldr's default services
 *        into the host's DI container.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Application/WebApplication.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Skirnir extension registering the framework's core services
     *        (router, middleware provider, parsers, loggers, server options,
     *        request id, etc.).
     *
     * Register on the @c skr::ApplicationBuilder before the host is built.
     */
    class BaldrExtension : public skr::IExtension
    {
      public:
        virtual ~BaldrExtension() = default;

        /**
         * @brief Add Baldr's default services to @p services.
         */
        virtual void ConfigureServices(
            skr::ServiceCollection& services) override;

        /**
         * @brief Resolve framework services from the built provider.
         */
        virtual void UseServices(
            skr::ServiceProvider& serviceProvider) override;
    };

} // namespace BALDR_NAMESPACE
