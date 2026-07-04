/**
 * @file Detail/Namespace.hpp
 * @brief Defines the @c BALDR_NAMESPACE macro used to qualify every public
 *        symbol in the Baldr library.
 *
 * The macro is self-bootstrap: on first inclusion @c BALDR_NAMESPACE is
 * undefined and the fallback expands to @c baldr. Subsequent inclusions see
 * the already-defined macro, leaving the namespace name untouched. Downstream
 * projects can override the namespace by defining @c BALDR_NAMESPACE on the
 * command line (or via Skirnir's mirror macro) before including any Baldr
 * header.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

namespace BALDR_NAMESPACE
{

#ifndef BALDR_NAMESPACE
    #define BALDR_NAMESPACE baldr
#endif

} // namespace BALDR_NAMESPACE