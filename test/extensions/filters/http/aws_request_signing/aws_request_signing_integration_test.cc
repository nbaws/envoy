// #include <chrono>

#include "source/common/common/logger.h"

#include "test/extensions/common/aws/mocks.h"
#include "test/integration/http_integration.h"
#include "test/test_common/utility.h"

using testing::NiceMock;
using testing::Return;

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Aws {
namespace {

const std::string AWS_REQUEST_SIGNING_CONFIG_SIGV4 = R"EOF(
name: envoy.filters.http.aws_request_signing
typed_config:
  "@type": type.googleapis.com/envoy.extensions.filters.http.aws_request_signing.v3.AwsRequestSigning
  service_name: vpc-lattice-svcs
  region: us-east-1
  signing_algorithm: aws_sigv4
  use_unsigned_payload: true
  match_excluded_headers:
  - prefix: x-envoy
  - prefix: x-forwarded
  - exact: x-amzn-trace-id
)EOF";

const std::string AWS_REQUEST_SIGNING_CONFIG_SIGV4A = R"EOF(
name: envoy.filters.http.aws_request_signing
typed_config:
  "@type": type.googleapis.com/envoy.extensions.filters.http.aws_request_signing.v3.AwsRequestSigning
  service_name: vpc-lattice-svcs
  region: '*'
  signing_algorithm: aws_sigv4a
  use_unsigned_payload: true
  match_excluded_headers:
  - prefix: x-envoy
  - prefix: x-forwarded
  - exact: x-amzn-trace-id
)EOF";

using Headers = std::vector<std::pair<const std::string, const std::string>>;

class AwsRequestSigningIntegrationTest : public testing::TestWithParam<Network::Address::IpVersion>,
                                         public HttpIntegrationTest {
public:
  AwsRequestSigningIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam()),
        credentials_("akid", "secret"),
        credentials_provider_(new NiceMock<MockCredentialsProvider>()) {
    skipPortUsageValidation();
  }

  void createUpstreams() override {
    HttpIntegrationTest::createUpstreams();
    addFakeUpstream(Http::CodecType::HTTP2);
  }

  void addUpstreamProtocolOptions() {
    config_helper_.addConfigModifier([&](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
      auto* cluster = bootstrap.mutable_static_resources()->mutable_clusters(0);

      ConfigHelper::HttpProtocolOptions protocol_options;
      protocol_options.mutable_upstream_http_protocol_options()->set_auto_sni(true);
      protocol_options.mutable_upstream_http_protocol_options()->set_auto_san_validation(true);
      protocol_options.mutable_explicit_http_config()->mutable_http_protocol_options();
      ConfigHelper::setProtocolOptions(*cluster, protocol_options);
    });
  }

protected:
  bool downstream_filter_ = true;
  Credentials credentials_;
  NiceMock<MockCredentialsProvider>* credentials_provider_;
};

INSTANTIATE_TEST_SUITE_P(IpVersions, AwsRequestSigningIntegrationTest,
                         testing::ValuesIn({Network::Address::IpVersion::v4}));

TEST_P(AwsRequestSigningIntegrationTest, SigV4IntegrationDownstream) {
  EXPECT_CALL(*credentials_provider_, getCredentials()).WillOnce(Return(Credentials()));

  config_helper_.prependFilter(AWS_REQUEST_SIGNING_CONFIG_SIGV4, true);
  HttpIntegrationTest::initialize();

  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":path", "/test/path"}, {":scheme", "http"}, {":authority", "host"}};

  auto response = sendRequestAndWaitForResponse(request_headers, 0, default_response_headers_, 0);

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response->complete());
  // check that our headers have been correctly added upstream
  EXPECT_FALSE(upstream_request_->headers().get(Http::LowerCaseString("authorization")).empty());
  EXPECT_FALSE(upstream_request_->headers().get(Http::LowerCaseString("x-amz-date")).empty());
  EXPECT_FALSE(
      upstream_request_->headers().get(Http::LowerCaseString("x-amz-security-token")).empty());
  EXPECT_FALSE(
      upstream_request_->headers().get(Http::LowerCaseString("x-amz-content-sha256")).empty());
}

TEST_P(AwsRequestSigningIntegrationTest, SigV4AIntegrationDownstream) {
  EXPECT_CALL(*credentials_provider_, getCredentials()).WillOnce(Return(Credentials()));

  config_helper_.prependFilter(AWS_REQUEST_SIGNING_CONFIG_SIGV4A, true);
  HttpIntegrationTest::initialize();

  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":path", "/test/path"}, {":scheme", "http"}, {":authority", "host"}};

  auto response = sendRequestAndWaitForResponse(request_headers, 0, default_response_headers_, 0);

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response->complete());
  // check that our headers have been correctly added upstream
  EXPECT_FALSE(upstream_request_->headers().get(Http::LowerCaseString("authorization")).empty());
  EXPECT_FALSE(upstream_request_->headers().get(Http::LowerCaseString("x-amz-date")).empty());
  EXPECT_FALSE(upstream_request_->headers().get(Http::LowerCaseString("x-amz-region-set")).empty());
  EXPECT_FALSE(
      upstream_request_->headers().get(Http::LowerCaseString("x-amz-security-token")).empty());
  EXPECT_FALSE(
      upstream_request_->headers().get(Http::LowerCaseString("x-amz-content-sha256")).empty());
}

TEST_P(AwsRequestSigningIntegrationTest, SigV4IntegrationUpstream) {
  EXPECT_CALL(*credentials_provider_, getCredentials()).WillOnce(Return(Credentials()));

  config_helper_.prependFilter(AWS_REQUEST_SIGNING_CONFIG_SIGV4, false);
  addUpstreamProtocolOptions();
  HttpIntegrationTest::initialize();

  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":path", "/test/path"}, {":scheme", "http"}, {":authority", "host"}};

  auto response = sendRequestAndWaitForResponse(request_headers, 0, default_response_headers_, 0);

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response->complete());
  // check that our headers have been correctly added upstream
  EXPECT_FALSE(upstream_request_->headers().get(Http::LowerCaseString("authorization")).empty());
  EXPECT_FALSE(upstream_request_->headers().get(Http::LowerCaseString("x-amz-date")).empty());
  EXPECT_FALSE(
      upstream_request_->headers().get(Http::LowerCaseString("x-amz-security-token")).empty());
  EXPECT_FALSE(
      upstream_request_->headers().get(Http::LowerCaseString("x-amz-content-sha256")).empty());
}

TEST_P(AwsRequestSigningIntegrationTest, SigV4AIntegrationUpstream) {
  EXPECT_CALL(*credentials_provider_, getCredentials()).WillOnce(Return(Credentials()));

  config_helper_.prependFilter(AWS_REQUEST_SIGNING_CONFIG_SIGV4A, false);
  addUpstreamProtocolOptions();
  HttpIntegrationTest::initialize();

  codec_client_ = makeHttpConnection(makeClientConnection((lookupPort("http"))));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":path", "/test/path"}, {":scheme", "http"}, {":authority", "host"}};

  auto response = sendRequestAndWaitForResponse(request_headers, 0, default_response_headers_, 0);

  EXPECT_TRUE(upstream_request_->complete());
  EXPECT_TRUE(response->complete());
  // check that our headers have been correctly added upstream
  EXPECT_FALSE(upstream_request_->headers().get(Http::LowerCaseString("authorization")).empty());
  EXPECT_FALSE(upstream_request_->headers().get(Http::LowerCaseString("x-amz-date")).empty());
  EXPECT_FALSE(upstream_request_->headers().get(Http::LowerCaseString("x-amz-region-set")).empty());
  EXPECT_FALSE(
      upstream_request_->headers().get(Http::LowerCaseString("x-amz-security-token")).empty());
  EXPECT_FALSE(
      upstream_request_->headers().get(Http::LowerCaseString("x-amz-content-sha256")).empty());
}

} // namespace
} // namespace Aws
} // namespace Common
} // namespace Extensions
} // namespace Envoy