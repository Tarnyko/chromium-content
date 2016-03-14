// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_

#include <set>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/test/test_utils.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
class WebContents;
struct LoadCommittedDetails;

// For browser_tests, which run on the UI thread, run a second
// MessageLoop and quit when the navigation completes loading.
class TestNavigationObserver {
 public:
  // Create and register a new TestNavigationObserver against the
  // |web_contents|.
  TestNavigationObserver(WebContents* web_contents,
                         int number_of_navigations);
  // Like above but waits for one navigation.
  explicit TestNavigationObserver(WebContents* web_contents);

  virtual ~TestNavigationObserver();

  // Runs a nested message loop and blocks until the expected number of
  // navigations are complete.
  void Wait();

  // Start/stop watching newly created WebContents.
  void StartWatchingNewWebContents();
  void StopWatchingNewWebContents();

  const GURL& last_navigation_url() const { return last_navigation_url_; }

  int last_navigation_succeeded() const { return last_navigation_succeeded_; }

 protected:
  // Register this TestNavigationObserver as an observer of the |web_contents|.
  void RegisterAsObserver(WebContents* web_contents);

 private:
  class TestWebContentsObserver;

  // Callbacks for WebContents-related events.
  void OnWebContentsCreated(WebContents* web_contents);
  void OnWebContentsDestroyed(TestWebContentsObserver* observer,
                              WebContents* web_contents);
  void OnNavigationEntryCommitted(
      TestWebContentsObserver* observer,
      WebContents* web_contents,
      const LoadCommittedDetails& load_details);
  void OnDidAttachInterstitialPage(WebContents* web_contents);
  void OnDidStartLoading(WebContents* web_contents);
  void OnDidStopLoading(WebContents* web_contents);
  void OnDidStartProvisionalLoad(RenderFrameHost* render_frame_host,
                                 const GURL& validated_url,
                                 bool is_error_page,
                                 bool is_iframe_srcdoc);
  void OnDidFailProvisionalLoad(RenderFrameHost* render_frame_host,
                                const GURL& validated_url,
                                int error_code,
                                const base::string16& error_description);
  void OnDidCommitProvisionalLoadForFrame(RenderFrameHost* render_frame_host,
                                          const GURL& url,
                                          ui::PageTransition transition_type);

  // If true the navigation has started.
  bool navigation_started_;

  // The number of navigations that have been completed.
  int navigations_completed_;

  // The number of navigations to wait for.
  int number_of_navigations_;

  // The url of the navigation that last committed.
  GURL last_navigation_url_;

  // True if the last navigation succeeded.
  bool last_navigation_succeeded_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  // Callback invoked on WebContents creation.
  base::Callback<void(WebContents*)> web_contents_created_callback_;

  // Living TestWebContentsObservers created by this observer.
  std::set<TestWebContentsObserver*> web_contents_observers_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigationObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_
