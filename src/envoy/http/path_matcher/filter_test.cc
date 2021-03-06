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

#include "src/envoy/http/path_matcher/filter.h"
#include "src/envoy/utils/filter_state_utils.h"
#include "test/mocks/server/mocks.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace PathMatcher {
namespace {

using Envoy::Http::MockStreamDecoderFilterCallbacks;
using Envoy::Server::Configuration::MockFactoryContext;
using ::google::protobuf::TextFormat;

const char kFilterConfig[] = R"(
rules {
  operation: "1.cloudesf_testing_cloud_goog.Bar"
  pattern {
    http_method: "GET"
    uri_template: "/bar"
  }
}
rules {
  operation: "1.cloudesf_testing_cloud_goog.Foo"
  extract_path_parameters: true
  pattern {
    http_method: "GET"
    uri_template: "/foo/{foo_bar}"
  }
}
segment_names {
  json_name: "fooBar"
  snake_name: "foo_bar"
})";

class FilterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ::google::api::envoy::http::path_matcher::FilterConfig config_pb;
    ASSERT_TRUE(TextFormat::ParseFromString(kFilterConfig, &config_pb));
    config_ =
        std::make_shared<FilterConfig>(config_pb, "", mock_factory_context_);

    filter_ = std::make_unique<Filter>(config_);
    filter_->setDecoderFilterCallbacks(mock_cb_);
  }

  std::unique_ptr<Filter> filter_;
  FilterConfigSharedPtr config_;
  testing::NiceMock<MockFactoryContext> mock_factory_context_;
  testing::NiceMock<MockStreamDecoderFilterCallbacks> mock_cb_;
};

TEST_F(FilterTest, DecodeHeadersWithOperation) {
  // Test: a request matches a operation
  Http::TestHeaderMapImpl headers{{":method", "GET"}, {":path", "/bar"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, false));

  EXPECT_EQ(Utils::getStringFilterState(mock_cb_.stream_info_.filter_state_,
                                        Utils::kOperation),
            "1.cloudesf_testing_cloud_goog.Bar");
  EXPECT_EQ(Utils::getStringFilterState(mock_cb_.stream_info_.filter_state_,
                                        Utils::kQueryParams),
            "");

  EXPECT_EQ(1L, TestUtility::findCounter(mock_factory_context_.scope_,
                                         "path_matcher.allowed")
                    ->value());
  EXPECT_EQ(0L, TestUtility::findCounter(mock_factory_context_.scope_,
                                         "path_matcher.denied")
                    ->value());

  Buffer::OwnedImpl data("");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, false));
  EXPECT_EQ(Http::FilterTrailersStatus::Continue,
            filter_->decodeTrailers(headers));
}

TEST_F(FilterTest, DecodeHeadersWithMethodOveride) {
  // Test: a request with a method override matches a operation
  Http::TestHeaderMapImpl headers{{":method", "POST"},
                                  {":path", "/bar"},
                                  {"x-http-method-override", "GET"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ(Utils::getStringFilterState(mock_cb_.stream_info_.filter_state_,
                                        Utils::kOperation),
            "1.cloudesf_testing_cloud_goog.Bar");

  EXPECT_EQ(1L, TestUtility::findCounter(mock_factory_context_.scope_,
                                         "path_matcher.allowed")
                    ->value());
  EXPECT_EQ(0L, TestUtility::findCounter(mock_factory_context_.scope_,
                                         "path_matcher.denied")
                    ->value());
}

TEST_F(FilterTest, DecodeHeadersExtractParameters) {
  // Test: a request needs to extract parameters
  Http::TestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo/123"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ(Utils::getStringFilterState(mock_cb_.stream_info_.filter_state_,
                                        Utils::kOperation),
            "1.cloudesf_testing_cloud_goog.Foo");
  EXPECT_EQ(Utils::getStringFilterState(mock_cb_.stream_info_.filter_state_,
                                        Utils::kQueryParams),
            "fooBar=123");

  EXPECT_EQ(1L, TestUtility::findCounter(mock_factory_context_.scope_,
                                         "path_matcher.allowed")
                    ->value());
  EXPECT_EQ(0L, TestUtility::findCounter(mock_factory_context_.scope_,
                                         "path_matcher.denied")
                    ->value());
}

TEST_F(FilterTest, DecodeHeadersNoMatch) {
  // Test: a request no match
  Http::TestHeaderMapImpl headers{{":method", "POST"}, {":path", "/bar"}};

  // Filter should reject this request
  EXPECT_CALL(
      mock_cb_.stream_info_,
      setResponseFlag(StreamInfo::ResponseFlag::UnauthorizedExternalService));

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ(Utils::getStringFilterState(mock_cb_.stream_info_.filter_state_,
                                        Utils::kOperation),
            "");

  EXPECT_EQ(0L, TestUtility::findCounter(mock_factory_context_.scope_,
                                         "path_matcher.allowed")
                    ->value());
  EXPECT_EQ(1L, TestUtility::findCounter(mock_factory_context_.scope_,
                                         "path_matcher.denied")
                    ->value());
}

}  // namespace

}  // namespace PathMatcher
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
