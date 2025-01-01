#include "source/extensions/filters/http/aws_request_signing/aws_request_signing_filter.h"

#include "envoy/extensions/filters/http/aws_request_signing/v3/aws_request_signing.pb.h"

#include "source/common/common/hex.h"
#include "source/common/crypto/utility.h"
#include "source/common/http/utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsRequestSigningFilter {

FilterConfigImpl::FilterConfigImpl(Extensions::Common::Aws::SignerPtr&& signer, 
Envoy::Extensions::Common::Aws::CredentialsProviderSharedPtr credentials_provider,
                                   const std::string& stats_prefix, Stats::Scope& scope,
                                   const std::string& host_rewrite, bool use_unsigned_payload)
    : signer_(std::move(signer)), credentials_provider_(credentials_provider), stats_(Filter::generateStats(stats_prefix, scope)),
      host_rewrite_(host_rewrite), use_unsigned_payload_{use_unsigned_payload} {}

Filter::Filter(const std::shared_ptr<FilterConfig>& config) : config_(config) {}

Extensions::Common::Aws::Signer& FilterConfigImpl::signer() { return *signer_; }

Envoy::Extensions::Common::Aws::CredentialsProviderSharedPtr FilterConfigImpl::credentialsProvider() { return credentials_provider_; }

FilterStats& FilterConfigImpl::stats() { return stats_; }

const std::string& FilterConfigImpl::hostRewrite() const { return host_rewrite_; }
bool FilterConfigImpl::useUnsignedPayload() const { return use_unsigned_payload_; }

FilterStats Filter::generateStats(const std::string& prefix, Stats::Scope& scope) {
  const std::string final_prefix = prefix + "aws_request_signing.";
  return {ALL_AWS_REQUEST_SIGNING_FILTER_STATS(POOL_COUNTER_PREFIX(scope, final_prefix))};
}

Http::FilterHeadersStatus Filter::onCredentialNoLongerPending(FilterConfig& config, Http::RequestHeaderMap& headers, bool end_stream, Envoy::Extensions::Common::Aws::Credentials credentials)
{
  ENVOY_LOG(debug, "aws request signing onCredentialNoLongerPending, {}",credentials.accessKeyId().value());

  const auto& host_rewrite = config.hostRewrite();
  const bool use_unsigned_payload = config.useUnsignedPayload();

  absl::Status status;

  if (!host_rewrite.empty()) {
    headers.setHost(host_rewrite);
  }

  if (!use_unsigned_payload && !end_stream) {
    request_headers_ = &headers;
    return Http::FilterHeadersStatus::StopIteration;
  }

  ENVOY_LOG(debug, "aws request signing from decodeHeaders use_unsigned_payload: {}",
            use_unsigned_payload);
  if (use_unsigned_payload) {
    status = config.signer().signUnsignedPayload(headers, config.credentialsProvider()->getCredentials());
  } else {
    status = config.signer().signEmptyPayload(headers, config.credentialsProvider()->getCredentials());
  }
  if (status.ok()) {
    config.stats().signing_added_.inc();
  } else {
    ENVOY_LOG(debug, "signing failed: {}", status.message());
    config.stats().signing_failed_.inc();
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) {
  auto& config = getConfig();
    ENVOY_LOG_MISC(debug, "******* HERE");

  if(config.credentialsProvider()->credentialsPending(
    [this, &dispatcher = decoder_callbacks_->dispatcher(), &end_stream, &headers, &config](Envoy::Extensions::Common::Aws::Credentials credentials) {
        dispatcher.post([this, &config, &headers, end_stream, credentials]() {
            this->onCredentialNoLongerPending(config, headers, end_stream, credentials);
        });
  }
  ))
  {
    ENVOY_LOG_MISC(debug, "Credentials are pending");
    return Http::FilterHeadersStatus::StopAllIterationAndBuffer;
  } else
  {
    ENVOY_LOG_MISC(debug, "Credentials are not pending");
  }
  return onCredentialNoLongerPending(config, headers, end_stream, config.credentialsProvider()->getCredentials());
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
  auto& config = getConfig();

  if (config.useUnsignedPayload()) {
    return Http::FilterDataStatus::Continue;
  }

  if (!end_stream) {
    return Http::FilterDataStatus::StopIterationAndBuffer;
  }

  decoder_callbacks_->addDecodedData(data, false);

  const Buffer::Instance& decoding_buffer = *decoder_callbacks_->decodingBuffer();

  auto& hashing_util = Envoy::Common::Crypto::UtilitySingleton::get();
  const std::string hash = Hex::encode(hashing_util.getSha256Digest(decoding_buffer));

  ENVOY_LOG(debug, "aws request signing from decodeData");
  ASSERT(request_headers_ != nullptr);
  auto status = config.signer().sign(*request_headers_, config.credentialsProvider()->getCredentials(), hash);
  if (status.ok()) {
    config.stats().signing_added_.inc();
    config.stats().payload_signing_added_.inc();
  } else {
    ENVOY_LOG(debug, "signing failed: {}", status.message());
    config.stats().signing_failed_.inc();
    config.stats().payload_signing_failed_.inc();
  }

  return Http::FilterDataStatus::Continue;
}

FilterConfig& Filter::getConfig() const {
  auto* config = const_cast<FilterConfig*>(
      Http::Utility::resolveMostSpecificPerFilterConfig<FilterConfig>(decoder_callbacks_));
  if (config) {
    return *config;
  }
  return *config_;
}

} // namespace AwsRequestSigningFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
