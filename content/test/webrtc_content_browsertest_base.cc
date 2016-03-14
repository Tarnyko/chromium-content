// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/webrtc_content_browsertest_base.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

void WebRtcContentBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnforceWebRtcIPPermissionCheck);

  // Loopback interface is the non-default local address. They should only be in
  // the candidate list if the ip handling policy is "default" AND the media
  // permission is granted.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLoopbackInPeerConnection);
}

void WebRtcContentBrowserTest::SetUp() {
  // We need pixel output when we dig pixels out of video tags for verification.
  EnablePixelOutput();
#if defined(OS_CHROMEOS)
    chromeos::CrasAudioHandler::InitializeForTesting();
#endif
  ContentBrowserTest::SetUp();
}

void WebRtcContentBrowserTest::TearDown() {
  ContentBrowserTest::TearDown();
#if defined(OS_CHROMEOS)
    chromeos::CrasAudioHandler::Shutdown();
#endif
}

void WebRtcContentBrowserTest::AppendUseFakeUIForMediaStreamFlag() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUseFakeUIForMediaStream);
}

// Executes |javascript|. The script is required to use
// window.domAutomationController.send to send a string value back to here.
std::string WebRtcContentBrowserTest::ExecuteJavascriptAndReturnResult(
    const std::string& javascript) {
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(), javascript, &result))
          << "Failed to execute javascript " << javascript << ".";
  return result;
}

void WebRtcContentBrowserTest::MakeTypicalCall(const std::string& javascript,
                                               const std::string& html_file) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL(html_file));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(javascript);
}

void WebRtcContentBrowserTest::ExecuteJavascriptAndWaitForOk(
    const std::string& javascript) {
   std::string result = ExecuteJavascriptAndReturnResult(javascript);
   if (result != "OK") {
     if (result.empty())
       result = "(nothing)";
     printf("From javascript: %s\nWhen executing '%s'\n", result.c_str(),
            javascript.c_str());
     FAIL();
   }
 }

std::string WebRtcContentBrowserTest::GenerateGetUserMediaCall(
    const char* function_name,
    int min_width,
    int max_width,
    int min_height,
    int max_height,
    int min_frame_rate,
    int max_frame_rate) const {
  return base::StringPrintf(
      "%s({video: {mandatory: {minWidth: %d, maxWidth: %d, "
      "minHeight: %d, maxHeight: %d, minFrameRate: %d, maxFrameRate: %d}, "
      "optional: []}});",
      function_name,
      min_width,
      max_width,
      min_height,
      max_height,
      min_frame_rate,
      max_frame_rate);
}

bool WebRtcContentBrowserTest::OnWinXp() const {
#if defined(OS_WIN)
  return base::win::GetVersion() <= base::win::VERSION_XP;
#else
  return false;
#endif
}

}  // namespace content
