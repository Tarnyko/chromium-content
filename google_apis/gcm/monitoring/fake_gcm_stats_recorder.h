// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_MONITORING_FAKE_GCM_STATS_RECODER_H_
#define GOOGLE_APIS_GCM_MONITORING_FAKE_GCM_STATS_RECODER_H_

#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"

namespace gcm {

// The fake version of GCMStatsRecorder that does nothing.
class FakeGCMStatsRecorder : public GCMStatsRecorder {
 public:
  FakeGCMStatsRecorder();
  ~FakeGCMStatsRecorder() override;

  void RecordCheckinInitiated(uint64 android_id) override;
  void RecordCheckinDelayedDueToBackoff(int64 delay_msec) override;
  void RecordCheckinSuccess() override;
  void RecordCheckinFailure(const std::string& status,
                            bool will_retry) override;
  void RecordConnectionInitiated(const std::string& host) override;
  void RecordConnectionDelayedDueToBackoff(int64 delay_msec) override;
  void RecordConnectionSuccess() override;
  void RecordConnectionFailure(int network_error) override;
  void RecordConnectionResetSignaled(
      ConnectionFactory::ConnectionResetReason reason) override;
  void RecordRegistrationSent(const std::string& app_id,
                              const std::string& source) override;
  void RecordRegistrationResponse(const std::string& app_id,
                                  const std::string& source,
                                  RegistrationRequest::Status status) override;
  void RecordRegistrationRetryDelayed(
      const std::string& app_id,
      const std::string& source,
      int64 delay_msec,
      int retries_left) override;
  void RecordUnregistrationSent(
      const std::string& app_id,
      const std::string& source) override;
  void RecordUnregistrationResponse(
      const std::string& app_id,
      const std::string& source,
      UnregistrationRequest::Status status) override;
  void RecordUnregistrationRetryDelayed(const std::string& app_id,
                                        const std::string& source,
                                        int64 delay_msec,
                                        int retries_left) override;
  void RecordDataMessageReceived(const std::string& app_id,
                                 const std::string& from,
                                 int message_byte_size,
                                 bool to_registered_app,
                                 ReceivedMessageType message_type) override;
  void RecordDataSentToWire(const std::string& app_id,
                            const std::string& receiver_id,
                            const std::string& message_id,
                            int queued) override;
  void RecordNotifySendStatus(const std::string& app_id,
                              const std::string& receiver_id,
                              const std::string& message_id,
                              MCSClient::MessageSendStatus status,
                              int byte_size,
                              int ttl) override;
  void RecordIncomingSendError(const std::string& app_id,
                               const std::string& receiver_id,
                               const std::string& message_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGCMStatsRecorder);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_MONITORING_FAKE_GCM_STATS_RECODER_H_
