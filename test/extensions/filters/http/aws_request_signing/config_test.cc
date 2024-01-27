#include "envoy/extensions/filters/http/aws_request_signing/v3/aws_request_signing.pb.h"
#include "envoy/extensions/filters/http/aws_request_signing/v3/aws_request_signing.pb.validate.h"

#include "source/extensions/filters/http/aws_request_signing/aws_request_signing_filter.h"
#include "source/extensions/filters/http/aws_request_signing/config.h"

#include "test/mocks/server/factory_context.h"
#include "test/test_common/environment.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsRequestSigningFilter {

TEST(AwsRequestSigningFilterConfigTest, SimpleConfig) {
  const std::string yaml = R"EOF(
service_name: s3
region: us-west-2
host_rewrite: new-host
match_excluded_headers:
  - prefix: x-envoy
  - exact: foo
  - exact: bar
  )EOF";

  AwsRequestSigningProtoConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);

  AwsRequestSigningProtoConfig expected_config;
  expected_config.set_service_name("s3");
  expected_config.set_region("us-west-2");
  expected_config.set_host_rewrite("new-host");
  expected_config.add_match_excluded_headers()->set_prefix("x-envoy");
  expected_config.add_match_excluded_headers()->set_exact("foo");
  expected_config.add_match_excluded_headers()->set_exact("bar");

  Protobuf::util::MessageDifferencer differencer;
  differencer.set_message_field_comparison(Protobuf::util::MessageDifferencer::EQUAL);
  differencer.set_repeated_field_comparison(Protobuf::util::MessageDifferencer::AS_SET);
  EXPECT_TRUE(differencer.Compare(expected_config, proto_config));

  testing::NiceMock<Server::Configuration::MockFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  Http::FilterFactoryCb cb =
      factory.createFilterFactoryFromProto(proto_config, "stats", context).value();
  Http::MockFilterChainFactoryCallbacks filter_callbacks;
  EXPECT_CALL(filter_callbacks, addStreamDecoderFilter(_));
  cb(filter_callbacks);
}

TEST(AwsRequestSigningFilterConfigTest, SimpleConfigExplicitSigningAlgorithm) {
  const std::string yaml = R"EOF(
service_name: s3
signing_algorithm: AWS_SIGV4
region: us-west-2
host_rewrite: new-host
match_excluded_headers:
  - prefix: x-envoy
  - exact: foo
  - exact: bar
  )EOF";

  AwsRequestSigningProtoConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);

  AwsRequestSigningProtoConfig expected_config;
  expected_config.set_service_name("s3");
  expected_config.set_region("us-west-2");
  expected_config.set_host_rewrite("new-host");
  expected_config.set_signing_algorithm(envoy::extensions::filters::http::aws_request_signing::v3::
                                            AwsRequestSigning_SigningAlgorithm_AWS_SIGV4);
  expected_config.add_match_excluded_headers()->set_prefix("x-envoy");
  expected_config.add_match_excluded_headers()->set_exact("foo");
  expected_config.add_match_excluded_headers()->set_exact("bar");

  Protobuf::util::MessageDifferencer differencer;
  differencer.set_message_field_comparison(Protobuf::util::MessageDifferencer::EQUAL);
  differencer.set_repeated_field_comparison(Protobuf::util::MessageDifferencer::AS_SET);
  EXPECT_TRUE(differencer.Compare(expected_config, proto_config));

  testing::NiceMock<Server::Configuration::MockFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  Http::FilterFactoryCb cb =
      factory.createFilterFactoryFromProto(proto_config, "stats", context).value();
  Http::MockFilterChainFactoryCallbacks filter_callbacks;
  EXPECT_CALL(filter_callbacks, addStreamDecoderFilter(_));
  cb(filter_callbacks);
}

TEST(AwsRequestSigningFilterConfigTest, InvalidRegionExplicitSigningAlgorithm) {
  const std::string yaml = R"EOF(
service_name: s3
signing_algorithm: AWS_SIGV4
region: us-west-1,us-west-2
host_rewrite: new-host
match_excluded_headers:
  - prefix: x-envoy
  - exact: foo
  - exact: bar
  )EOF";

  AwsRequestSigningProtoConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);

  testing::NiceMock<Server::Configuration::MockFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  EXPECT_THROW(
      {
        Http::FilterFactoryCb cb =
            factory.createFilterFactoryFromProto(proto_config, "stats", context).value();
        Http::MockFilterChainFactoryCallbacks filter_callbacks;
        cb(filter_callbacks);
      },
      EnvoyException);
}

TEST(AwsRequestSigningFilterConfigTest, SimpleConfigSigV4A) {
  const std::string yaml = R"EOF(
service_name: s3
region: '*'
host_rewrite: new-host
signing_algorithm: AWS_SIGV4A
match_excluded_headers:
  - prefix: x-envoy
  - exact: foo
  - exact: bar
  )EOF";

  AwsRequestSigningProtoConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);

  AwsRequestSigningProtoConfig expected_config;
  expected_config.set_service_name("s3");
  expected_config.set_region("*");
  expected_config.set_host_rewrite("new-host");
  expected_config.set_signing_algorithm(envoy::extensions::filters::http::aws_request_signing::v3::
                                            AwsRequestSigning_SigningAlgorithm_AWS_SIGV4A);
  expected_config.add_match_excluded_headers()->set_prefix("x-envoy");
  expected_config.add_match_excluded_headers()->set_exact("foo");
  expected_config.add_match_excluded_headers()->set_exact("bar");

  Protobuf::util::MessageDifferencer differencer;
  differencer.set_message_field_comparison(Protobuf::util::MessageDifferencer::EQUAL);
  differencer.set_repeated_field_comparison(Protobuf::util::MessageDifferencer::AS_SET);
  EXPECT_TRUE(differencer.Compare(expected_config, proto_config));

  testing::NiceMock<Server::Configuration::MockFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  Http::FilterFactoryCb cb =
      factory.createFilterFactoryFromProto(proto_config, "stats", context).value();
  Http::MockFilterChainFactoryCallbacks filter_callbacks;
  EXPECT_CALL(filter_callbacks, addStreamDecoderFilter(_));
  cb(filter_callbacks);
}

TEST(AwsRequestSigningFilterConfigTest, RouteSpecificFilterConfigSigV4) {
  const std::string yaml = R"EOF(
aws_request_signing:
  service_name: s3
  signing_algorithm: AWS_SIGV4
  region: us-west-2
  host_rewrite: new-host
  match_excluded_headers:
    - prefix: x-envoy
    - exact: foo
    - exact: bar
stat_prefix: foo_prefix
  )EOF";

  AwsRequestSigningProtoPerRouteConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);

  testing::NiceMock<Server::Configuration::MockServerFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      proto_config, context, ProtobufMessage::getNullValidationVisitor());
  ASSERT_NE(route_config, nullptr);
}

TEST(AwsRequestSigningFilterConfigTest, RouteSpecificFilterConfigSigV4RegionInEnv) {
  const std::string yaml = R"EOF(
aws_request_signing:
  service_name: s3
  signing_algorithm: AWS_SIGV4
  host_rewrite: new-host
  match_excluded_headers:
    - prefix: x-envoy
    - exact: foo
    - exact: bar
stat_prefix: foo_prefix
  )EOF";

  TestEnvironment::setEnvVar("AWS_REGION", "ap-southeast-2", 1);

  AwsRequestSigningProtoPerRouteConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);

  testing::NiceMock<Server::Configuration::MockServerFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      proto_config, context, ProtobufMessage::getNullValidationVisitor());
  ASSERT_NE(route_config, nullptr);
}

TEST(AwsRequestSigningFilterConfigTest, RouteSpecificFilterConfigSigV4A) {
  const std::string yaml = R"EOF(
aws_request_signing:
  service_name: s3
  signing_algorithm: AWS_SIGV4A
  region: '*'
  host_rewrite: new-host
  match_excluded_headers:
    - prefix: x-envoy
    - exact: foo
    - exact: bar
stat_prefix: foo_prefix
  )EOF";

  AwsRequestSigningProtoPerRouteConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);

  testing::NiceMock<Server::Configuration::MockServerFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      proto_config, context, ProtobufMessage::getNullValidationVisitor());
  ASSERT_NE(route_config, nullptr);
}

TEST(AwsRequestSigningFilterConfigTest, InvalidRegionRouteSpecificFilterConfigSigV4) {
  const std::string yaml = R"EOF(
aws_request_signing:
  service_name: s3
  signing_algorithm: AWS_SIGV4
  region: '*'
  host_rewrite: new-host
  match_excluded_headers:
    - prefix: x-envoy
    - exact: foo
    - exact: bar
stat_prefix: foo_prefix
  )EOF";

  AwsRequestSigningProtoPerRouteConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);

  testing::NiceMock<Server::Configuration::MockServerFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  EXPECT_THROW(
      {
        const auto route_config = factory.createRouteSpecificFilterConfig(
            proto_config, context, ProtobufMessage::getNullValidationVisitor());
      },
      EnvoyException);
}

TEST(AwsRequestSigningFilterConfigTest, SimpleConfigSigV4RegionInEnv) {

  const std::string yaml = R"EOF(
service_name: s3
host_rewrite: new-host
match_excluded_headers:
  - prefix: x-envoy
  - exact: foo
  - exact: bar
  )EOF";

  AwsRequestSigningProtoConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);
  TestEnvironment::setEnvVar("AWS_REGION", "ap-southeast-2", 1);

  testing::NiceMock<Server::Configuration::MockFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  Http::FilterFactoryCb cb =
      factory.createFilterFactoryFromProto(proto_config, "stats", context).value();
  Http::MockFilterChainFactoryCallbacks filter_callbacks;
  EXPECT_CALL(filter_callbacks, addStreamDecoderFilter(_));
  cb(filter_callbacks);
}

TEST(AwsRequestSigningFilterConfigTest, InvalidConfigSigV4ARegionInEnv) {
  const std::string yaml = R"EOF(
service_name: s3
signing_algorithm: AWS_SIGV4A
host_rewrite: new-host
match_excluded_headers:
  - prefix: x-envoy
  - exact: foo
  - exact: bar
  )EOF";

  AwsRequestSigningProtoConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);
  TestEnvironment::setEnvVar("AWS_REGION", "ap-southeast-2", 1);

  testing::NiceMock<Server::Configuration::MockFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  // Should fail as sigv4a requires region explicitly within config
  EXPECT_THROW(
      {
        Http::FilterFactoryCb cb =
            factory.createFilterFactoryFromProto(proto_config, "stats", context).value();
        Http::MockFilterChainFactoryCallbacks filter_callbacks;
        cb(filter_callbacks);
      },
      EnvoyException);
}

TEST(AwsRequestSigningFilterConfigTest, UpstreamFactoryTest) {

  auto* factory =
      Registry::FactoryRegistry<Server::Configuration::UpstreamHttpFilterConfigFactory>::getFactory(
          "envoy.filters.http.aws_request_signing");
  ASSERT_NE(factory, nullptr);
}

TEST(AwsRequestSigningFilterConfigTest, InvalidRegionRouteSpecificFilterConfigNoRegionAvailable) {
  const std::string yaml = R"EOF(
aws_request_signing:
  service_name: s3
  signing_algorithm: AWS_SIGV4
  host_rewrite: new-host
  match_excluded_headers:
    - prefix: x-envoy
    - exact: foo
    - exact: bar
stat_prefix: foo_prefix
  )EOF";

  TestEnvironment::unsetEnvVar("HOME");
  TestEnvironment::unsetEnvVar("AWS_CONFIG");
  TestEnvironment::unsetEnvVar("AWS_PROFILE");
  TestEnvironment::unsetEnvVar("AWS_REGION");
  TestEnvironment::unsetEnvVar("AWS_DEFAULT_REGION");
  TestEnvironment::unsetEnvVar("AWS_SHARED_CREDENTIALS_FILE");

  AwsRequestSigningProtoPerRouteConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);

  testing::NiceMock<Server::Configuration::MockServerFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  EXPECT_THROW(
      {
        const auto route_config = factory.createRouteSpecificFilterConfig(
            proto_config, context, ProtobufMessage::getNullValidationVisitor());
      },
      EnvoyException);
}

TEST(AwsRequestSigningFilterConfigTest, InvalidRegionNoRegionAvailable) {
  const std::string yaml = R"EOF(
service_name: s3
signing_algorithm: AWS_SIGV4
host_rewrite: new-host
match_excluded_headers:
  - prefix: x-envoy
  - exact: foo
  - exact: bar
  )EOF";

  TestEnvironment::unsetEnvVar("HOME");
  TestEnvironment::unsetEnvVar("AWS_CONFIG");
  TestEnvironment::unsetEnvVar("AWS_PROFILE");
  TestEnvironment::unsetEnvVar("AWS_REGION");
  TestEnvironment::unsetEnvVar("AWS_DEFAULT_REGION");
  TestEnvironment::unsetEnvVar("AWS_SHARED_CREDENTIALS_FILE");

  AwsRequestSigningProtoConfig proto_config;
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);

  testing::NiceMock<Server::Configuration::MockFactoryContext> context;
  AwsRequestSigningFilterFactory factory;

  // Should fail as sigv4a requires region explicitly within config
  EXPECT_THROW(
      {
        Http::FilterFactoryCb cb =
            factory.createFilterFactoryFromProto(proto_config, "stats", context).value();
        Http::MockFilterChainFactoryCallbacks filter_callbacks;
        cb(filter_callbacks);
      },
      EnvoyException);
}


} // namespace AwsRequestSigningFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
