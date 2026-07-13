/*
 * Copyright 2024 Toyota Connected North America
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FLUTTER_PLUGIN_COMMON_LOGGING_H_
#define FLUTTER_PLUGIN_COMMON_LOGGING_H_

// Plugin logging routes through ivi-homescreen's ihs_shared logging surface:
// ihs::log::{trace,debug,info,warn,error,critical} plus the IHS_DEBUG /
// IHS_TRACE macros (compiled out under NDEBUG, as the old SPDLOG_DEBUG /
// SPDLOG_TRACE were). Previously this header configured and included spdlog.
#include "logging/logging.h"

// IHS_LOG_SHIM (set by plugins/CMakeLists.txt's shell/logging/logging.h text
// probe) means this shell pin predates the ihs_shared rewrite: no
// `namespace ihs::log`, no IHS_DEBUG/IHS_TRACE. shell/logging/logging.h at
// these older pins still sets up spdlog directly (spdlog::info/warn/error/
// trace/critical via the global default logger, LOG_*/DLOG_* macros) --
// alias ihs::log onto it 1:1 (identical fmt-style format-string + args
// signatures) so plugin code using ihs::log::* keeps working unchanged
// against either shell generation.
#if defined(IHS_LOG_SHIM)
namespace ihs {
namespace log {
using namespace spdlog;
}  // namespace log
}  // namespace ihs

#ifndef IHS_DEBUG
#define IHS_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#endif
#ifndef IHS_TRACE
#define IHS_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#endif
#endif  // IHS_LOG_SHIM

#endif  // FLUTTER_PLUGIN_COMMON_LOGGING_H_
