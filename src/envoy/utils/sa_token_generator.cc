// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/envoy/utils/sa_token_generator.h"
#include "src/api_proxy/auth/auth_token.h"

namespace Envoy {
namespace Extensions {
namespace Utils {
namespace {
// Token expired in 1 hour, reduce 5 seconds for grace buffer.
const std::chrono::seconds kRefresherDefaultTokenExpiry(3600 - 5);

}  // namespace

ServiceAccountTokenGenerator::ServiceAccountTokenGenerator(
    Envoy::Server::Configuration::FactoryContext& context,
    const std::string& service_account_key, const std::string& audience,
    TokenUpdateFunc callback)
    : service_account_key_(service_account_key),
      audience_(audience),
      callback_(callback) {
  refresh_timer_ =
      context.dispatcher().createTimer([this]() -> void { refresh(); });
  // call this in a delayed fashion so that the callback is ready to accept new
  // token.
  context.dispatcher().post([this]() -> void { refresh(); });
}

void ServiceAccountTokenGenerator::refresh() {
  char* token = ::google::api_proxy::auth::get_auth_token(
      service_account_key_.c_str(), audience_.c_str());
  callback_(token);
  ENVOY_LOG(debug, "Generated token: {}", token);
  ::google::api_proxy::auth::grpc_free(token);

  // Update the token every 1 hour.
  refresh_timer_->enableTimer(kRefresherDefaultTokenExpiry);
}

}  // namespace Utils
}  // namespace Extensions
}  // namespace Envoy
