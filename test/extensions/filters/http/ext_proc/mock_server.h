#pragma once

#include "source/extensions/filters/http/ext_proc/client.h"

#include "gmock/gmock.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ExternalProcessing {

class MockClient : public ExternalProcessorClient {
public:
  MockClient();
  ~MockClient() override;

  MOCK_METHOD(ExternalProcessorStreamPtr, start,
              (ExternalProcessorCallbacks&, const Grpc::GrpcServiceConfigWithHashKey&,
               Envoy::Http::AsyncClient::StreamOptions&,
               Envoy::Http::StreamFilterSidestreamWatermarkCallbacks&));
  MOCK_METHOD(void, sendRequest,
              (envoy::service::ext_proc::v3::ProcessingRequest&&, bool, const uint64_t,
               RequestCallbacks*, StreamBase*));
  MOCK_METHOD(void, cancel, ());

  MOCK_METHOD(const Envoy::StreamInfo::StreamInfo*, getStreamInfo, (), (const));
};

class MockStream : public ExternalProcessorStream {
public:
  MockStream();
  ~MockStream() override;
  MOCK_METHOD(void, send, (envoy::service::ext_proc::v3::ProcessingRequest&&, bool));
  MOCK_METHOD(bool, closeLocalStream, ());
  MOCK_METHOD(const StreamInfo::StreamInfo&, streamInfo, (), (const override));
  MOCK_METHOD(StreamInfo::StreamInfo&, streamInfo, ());
  MOCK_METHOD(void, notifyFilterDestroy, ());
  MOCK_METHOD(bool, remoteClosed, (), (const override));
  MOCK_METHOD(bool, localClosed, (), (const override));
  MOCK_METHOD(void, resetStream, ());
  // Whether close() has been called.
  bool local_closed_ = false;
  bool remote_closed_ = false;
};

} // namespace ExternalProcessing
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
