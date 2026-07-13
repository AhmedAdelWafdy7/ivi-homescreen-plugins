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

#endif  // FLUTTER_PLUGIN_COMMON_LOGGING_H_
